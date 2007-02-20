/*
    gwaccount.cpp - Kopete GroupWise Protocol

    Copyright (c) 2006      Novell, Inc	 	 http://www.opensuse.org
    Copyright (c) 2004      SUSE Linux AG	 	 http://www.suse.com

    Based on Testbed
    Copyright (c) 2003      by Will Stephenson		 <will@stevello.free-online.co.uk>

    Kopete    (c) 2002-2003 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU General Public                   *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include <sys/utsname.h>

#include <qvalidator.h>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kinputdialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpassivepopup.h>

#include <kopeteuiglobal.h>
#include <kopeteaway.h>
#include <kopeteawayaction.h>
#include <kopetecontactlist.h>
#include <kopetegroup.h>
#include <kopeteglobal.h>
#include <kopetemetacontact.h>
#include <kopetepassword.h>
#include <kopeteview.h>

#include "client.h"
#include <QtCrypto>
#include "gwcontact.h"
#include "gwcontactlist.h"
#include "gwprotocol.h"
#include "gwconnector.h"
#include "gwmessagemanager.h"
#include "privacymanager.h"
#include "qcatlshandler.h"
#include "userdetailsmanager.h"
#include "tasks/createcontacttask.h"
#include "tasks/createcontactinstancetask.h"
#include "tasks/deleteitemtask.h"
#include "tasks/movecontacttask.h"
#include "tasks/updatecontacttask.h"
#include "tasks/updatefoldertask.h"
#include "ui/gwchatsearchdialog.h"
#include "ui_gwprivacy.h"
#include "ui/gwprivacydialog.h"
#include "ui/gwreceiveinvitationdialog.h"

#include "gwaccount.h"

GroupWiseAccount::GroupWiseAccount( GroupWiseProtocol *parent, const QString& accountID, const char *name )
: Kopete::PasswordedAccount ( parent, accountID )
{
	Q_UNUSED( name );
	// Init the myself contact
	setMyself( new GroupWiseContact( this, accountId(), Kopete::ContactList::self()->myself(), 0, 0, 0 ) );
	myself()->setOnlineStatus( GroupWiseProtocol::protocol()->groupwiseOffline );

	// Contact list management
	QObject::connect( Kopete::ContactList::self(), SIGNAL( groupRenamed( Kopete::Group *, const QString & ) ),
			SLOT( slotKopeteGroupRenamed( Kopete::Group * ) ) );
	QObject::connect( Kopete::ContactList::self(), SIGNAL( groupRemoved( Kopete::Group * ) ),
			SLOT( slotKopeteGroupRemoved( Kopete::Group * ) ) );

//		m_actionBlock = new KAction( KIcon( "msn_blocked" ), label, 0, "actionBlock" );
//		QObject::connect( m_actionBlock, SIGNAL( triggered( bool ) ), SLOT( slotBlock() ) );
	m_actionAutoReply = new KAction ( i18n( "&Set Auto-Reply..." ), 0 );
	QObject::connect( m_actionAutoReply, SIGNAL( triggered( bool ) ),
			SLOT( slotSetAutoReply() ) );
	m_actionJoinChatRoom = new KAction ( i18n( "&Join Channel..." ), 0 );
	QObject::connect( m_actionJoinChatRoom, SIGNAL( triggered( bool ) ),
										 SLOT( slotJoinChatRoom() ) );
	m_actionManagePrivacy = new KAction ( i18n( "&Manage Privacy..." ), 0 );
	QObject::connect( m_actionManagePrivacy, SIGNAL( triggered( bool ) ),
										 SLOT( slotPrivacy() ) );
			
	m_connector = 0;
	m_QCATLS = 0;
	m_tlsHandler = 0;
	m_clientStream = 0;
	m_client = 0;
	m_dontSync = false;
	m_serverListModel = 0;
}

GroupWiseAccount::~GroupWiseAccount()
{
	cleanup();
}

KActionMenu* GroupWiseAccount::actionMenu()
{
	KActionMenu *m_actionMenu=Kopete::Account::actionMenu();

	m_actionAutoReply->setEnabled( isConnected() );
	m_actionManagePrivacy->setEnabled( isConnected() );
	m_actionMenu->addAction( m_actionManagePrivacy );
	m_actionMenu->addAction( m_actionAutoReply );
	m_actionMenu->addAction( m_actionJoinChatRoom );
	/* Used for debugging */
	/*
	theActionMenu->insert( new KAction ( "Test rtfize()", QString::null, 0, this,
		SLOT( slotTestRTFize() ), this,
		"actionTestRTFize") );
	*/
	return m_actionMenu;
}

const int GroupWiseAccount::port() const
{
	return configGroup()->readEntry( "Port", 0 );
}

const QString GroupWiseAccount::server() const
{
	return configGroup()->readEntry( "Server", "" );
}

Client * GroupWiseAccount::client() const
{
	return m_client;
}

GroupWiseProtocol *GroupWiseAccount::protocol() const
{
	return static_cast<GroupWiseProtocol *>( Kopete::Account::protocol() );
}

GroupWiseChatSession * GroupWiseAccount::chatSession( Kopete::ContactPtrList others, const GroupWise::ConferenceGuid & guid, Kopete::Contact::CanCreateFlags canCreate )
{
	GroupWiseChatSession * chatSession = 0;
	do // one iteration misuse of do...while to enable an easy drop-out once we locate a manager
	{
		// do we have a manager keyed by GUID?
		if ( !guid.isEmpty() )
		{
			chatSession = findChatSessionByGuid( guid );
			if ( chatSession )
			{
					kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " found a message manager by GUID: " << guid << endl;
					break;
			}
		}
		// does the factory know about one, going on the chat members?
		chatSession = dynamic_cast<GroupWiseChatSession*>(
				Kopete::ChatSessionManager::self()->findChatSession( myself(), others, protocol() ) );
		if ( chatSession )
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " found a message manager by members with GUID: " << chatSession->guid() << endl;
			// re-add the returning contact(s) (very likely only one) to the chat
			foreach ( Kopete::Contact * returningContact, others )
				chatSession->joined( static_cast<GroupWiseContact *>( returningContact ) );

			if ( !guid.isEmpty() )
				chatSession->setGuid( guid );
			break;
		}
		// we don't have an existing message manager for this chat, so create one if we may
		if ( canCreate )
		{
			chatSession = new GroupWiseChatSession( myself(), others, protocol(), guid );
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo <<
					" created a new message manager with GUID: " << chatSession->guid() << endl;
			m_chatSessions.append( chatSession );
			// listen for the message manager telling us that the user
			//has left the conference so we remove it from our map
			QObject::connect( chatSession, SIGNAL( leavingConference( GroupWiseChatSession * ) ),
							SLOT( slotLeavingConference( GroupWiseChatSession * ) ) );
			break;
		}
		//kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo <<
		//		" no message manager available." << endl;
	}
	while ( 0 );
	//dumpManagers();
	return chatSession;
}

GroupWiseChatSession * GroupWiseAccount::findChatSessionByGuid( const GroupWise::ConferenceGuid & guid )
{
	GroupWiseChatSession * chatSession = 0;
	Q3ValueList<GroupWiseChatSession *>::ConstIterator it;
	for ( it = m_chatSessions.begin(); it != m_chatSessions.end(); ++it )
	{
		if ( (*it)->guid() == guid )
		{
				chatSession = *it;
				break;
		}
	}
	return chatSession;
}

GroupWiseContact * GroupWiseAccount::contactForDN( const QString & dn )
{
	QHashIterator<QString, Kopete::Contact*> i( contacts() );
	// check if we have a DN for them
	while ( i.hasNext() )
	{
		i.next();
		GroupWiseContact * candidate = static_cast<GroupWiseContact*>( i.value() );
		if ( candidate && candidate->dn() == dn )
			return candidate;
	}
	// we might have just added the contact with a user ID, try the first section of the dotted dn
	return static_cast< GroupWiseContact * >( contacts()[ protocol()->dnToDotted( dn ).section( '.', 0, 0 ) ] );
}

void GroupWiseAccount::setAway( bool away, const QString & reason )
{
	if ( away )
	{
		if ( Kopete::Away::getInstance()->idleTime() > 10 ) // don't go AwayIdle unless the user has actually been idle this long
			setOnlineStatus( protocol()->groupwiseAwayIdle );
		else
			setOnlineStatus( protocol()->groupwiseAway, reason );
	}
	else
		setOnlineStatus( protocol()->groupwiseAvailable );
}

void GroupWiseAccount::connectWithPassword( const QString &password )
{
	if ( password.isEmpty() )
	{
		disconnect();
		return;
	}
	// don't try and connect if we are already connected
	if ( isConnected () )
		return;

	bool sslPossible = QCA::isSupported("tls");

	if (!sslPossible)
	{
		KMessageBox::queuedMessageBox(Kopete::UI::Global::mainWidget (), KMessageBox::Error,
							i18n ("SSL support could not be initialized for account %1. This is most likely because the QCA TLS plugin is not installed on your system.", 
							myself()->contactId()),
							i18n ("GroupWise SSL Error"));
		return;
	}
	if ( m_client )
	{
		m_client->close();
		cleanup();
	}
	// set up network classes
	m_connector = new KNetworkConnector( 0 );
	//myConnector->setOptHostPort( "localhost", 8300 );
	m_connector->setOptHostPort( server(), port() );
	m_connector->setOptSSL( true );
	Q_ASSERT( QCA::isSupported("tls") );
	m_QCATLS = new QCA::TLS;
	m_tlsHandler = new QCATLSHandler( m_QCATLS );
	m_clientStream = new ClientStream( m_connector, m_tlsHandler, 0);

	QObject::connect( m_connector, SIGNAL( error() ), this, SLOT( slotConnError() ) );
	QObject::connect( m_connector, SIGNAL( connected() ), this, SLOT( slotConnConnected() ) );

	QObject::connect (m_clientStream, SIGNAL (connectionClosed()),
				this, SLOT (slotCSDisconnected()));
	QObject::connect (m_clientStream, SIGNAL (delayedCloseFinished()),
				this, SLOT (slotCSDisconnected()));
	// Notify us when the transport layer is connected
	QObject::connect( m_clientStream, SIGNAL( connected() ), SLOT( slotCSConnected() ) );
	// it's necessary to catch this signal and tell the TLS handler to proceed
	// even if we don't check cert validity
	QObject::connect( m_tlsHandler, SIGNAL(tlsHandshaken()), SLOT( slotTLSHandshaken()) );
	// starts the client once the security layer is up, but see below
	QObject::connect( m_clientStream, SIGNAL( securityLayerActivated(int) ), SLOT( slotTLSReady(int) ) );
	// we could handle login etc in start(), in which case we would emit this signal after that
	//QObject::connect (jabberClientStream, SIGNAL (authenticated()),
	//			this, SLOT (slotCSAuthenticated ()));
	// we could also get do the actual login in response to this..
	//QObject::connect (m_clientStream, SIGNAL (needAuthParams(bool, bool, bool)),
	//			this, SLOT (slotCSNeedAuthParams (bool, bool, bool)));

	// not implemented: warning
	QObject::connect( m_clientStream, SIGNAL( warning(int) ), SLOT( slotCSWarning(int) ) );
	// not implemented: error
	QObject::connect( m_clientStream, SIGNAL( error(int) ), SLOT( slotCSError(int) ) );

	m_client = new Client( this, CMSGPRES_GW_6_5 );

	// NB these are prefixed with QObject:: to avoid any chance of a clash with our connect() methods.
	// we connected successfully
	QObject::connect( m_client, SIGNAL( loggedIn() ), SLOT( slotLoggedIn() ) );
	// or connection failed
	QObject::connect( m_client, SIGNAL( loginFailed() ), SLOT( slotLoginFailed() ) );
	// folder listed
	QObject::connect( m_client, SIGNAL( folderReceived( const FolderItem & ) ), SLOT( receiveFolder( const FolderItem & ) ) );
	// contact listed
	QObject::connect( m_client, SIGNAL( contactReceived( const ContactItem & ) ), SLOT( receiveContact( const ContactItem & ) ) );
	// contact details listed
	QObject::connect( m_client, SIGNAL( contactUserDetailsReceived( const GroupWise::ContactDetails & ) ), SLOT( receiveContactUserDetails( const GroupWise::ContactDetails & ) ) );
	// contact status changed
	QObject::connect( m_client, SIGNAL( statusReceived( const QString &, quint16, const QString & ) ), SLOT( receiveStatus( const QString &, quint16 , const QString & ) ) );
	// incoming message
	QObject::connect( m_client, SIGNAL( messageReceived( const ConferenceEvent & ) ), SLOT( handleIncomingMessage( const ConferenceEvent & ) ) );
	// auto reply to one of our messages because the recipient is away
	QObject::connect( m_client, SIGNAL( autoReplyReceived( const ConferenceEvent & ) ), SLOT( handleIncomingMessage( const ConferenceEvent & ) ) );

	QObject::connect( m_client, SIGNAL( ourStatusChanged( GroupWise::Status, const QString &, const QString & ) ), SLOT( changeOurStatus( GroupWise::Status, const QString &, const QString & ) ) );
	// conference events
	QObject::connect( m_client,
		SIGNAL( conferenceCreated( const int, const GroupWise::ConferenceGuid & ) ),
		SIGNAL( conferenceCreated( const int, const GroupWise::ConferenceGuid & ) ) );
	QObject::connect( m_client, SIGNAL( conferenceCreationFailed( const int,  const int ) ), SIGNAL( conferenceCreationFailed( const int,  const int ) ) );
	QObject::connect( m_client, SIGNAL( invitationReceived( const ConferenceEvent & ) ), SLOT( receiveInvitation( const ConferenceEvent & ) ) );
	QObject::connect( m_client, SIGNAL( conferenceLeft( const ConferenceEvent & ) ), SLOT( receiveConferenceLeft( const ConferenceEvent & ) ) );
	QObject::connect( m_client, SIGNAL( conferenceJoinNotifyReceived( const ConferenceEvent & ) ), SLOT( receiveConferenceJoinNotify( const ConferenceEvent & ) ) );
	QObject::connect( m_client, SIGNAL( inviteNotifyReceived( const ConferenceEvent & ) ), SLOT( receiveInviteNotify( const ConferenceEvent & ) ) );
	QObject::connect( m_client, SIGNAL( invitationDeclined( const ConferenceEvent & ) ), SLOT( receiveInviteDeclined( const ConferenceEvent & ) ) );

	QObject::connect( m_client, SIGNAL( conferenceJoined( const GroupWise::ConferenceGuid &, const QStringList &, const QStringList &  ) ), SLOT( receiveConferenceJoin( const GroupWise::ConferenceGuid &, const QStringList & , const QStringList & ) ) );

	// typing events
	QObject::connect( m_client, SIGNAL( contactTyping( const ConferenceEvent & ) ),
								SIGNAL( contactTyping( const ConferenceEvent & ) ) );
	QObject::connect( m_client, SIGNAL( contactNotTyping( const ConferenceEvent & ) ),
								SIGNAL( contactNotTyping( const ConferenceEvent & ) ) );
	// misc
	QObject::connect( m_client, SIGNAL( accountDetailsReceived( const GroupWise::ContactDetails &) ), SLOT( receiveAccountDetails( const GroupWise::ContactDetails & ) ) );
	QObject::connect( m_client, SIGNAL( connectedElsewhere() ), SLOT( slotConnectedElsewhere() ) );
	// privacy - contacts can't connect directly to this signal because myself() is initialised before m_client
	QObject::connect( m_client->privacyManager(), SIGNAL( privacyChanged( const QString &, bool ) ), SIGNAL( privacyChanged( const QString &, bool ) ) );

	// GW7
	QObject::connect( m_client, SIGNAL( broadcastReceived( const ConferenceEvent & ) ), SLOT( handleIncomingMessage( const ConferenceEvent & ) ) );
	QObject::connect( m_client, SIGNAL( systemBroadcastReceived( const ConferenceEvent & ) ), SLOT( handleIncomingMessage( const ConferenceEvent & ) ) );

	struct utsname utsBuf;
	uname (&utsBuf);
	m_client->setClientName ("Kopete");
	m_client->setClientVersion ( KGlobal::mainComponent().aboutData()->version () );
	m_client->setOSName (QString ("%1 %2").arg (utsBuf.sysname, 1).arg (utsBuf.release, 2));

	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "Connecting to GroupWise server " << server() << ':' << port() << endl;

	NovellDN dn;
	dn.dn = "maeuschen";
	dn.server = "reiser.suse.de";
	m_serverListModel = new GWContactList( this );
	myself()->setOnlineStatus( protocol()->groupwiseConnecting );
	m_client->connectToServer( m_clientStream, dn, true );

}

void GroupWiseAccount::setOnlineStatus( const Kopete::OnlineStatus& status, const Kopete::StatusMessage &reason )
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	if ( status == protocol()->groupwiseUnknown
			|| status == protocol()->groupwiseConnecting
			|| status == protocol()->groupwiseInvalid )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " called with invalid status \"" 
				<< status.description() << "\"" << endl;
	}
	// going offline
	else if ( status == protocol()->groupwiseOffline )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " DISCONNECTING" << endl;
		disconnect();
	}
	// changing status
	else if ( isConnected() )
	{
		kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "changing status to \"" << status.description() << "\"" << endl;
		// Appear Offline is achieved by explicitly setting the status to offline, 
		// rather than disconnecting as when really going offline.
		if ( status == protocol()->groupwiseAppearOffline )
			m_client->setStatus( GroupWise::Offline, reason.message(), configGroup()->readEntry( "AutoReply", "" ) );
		else
			m_client->setStatus( ( GroupWise::Status )status.internalStatus(), reason.message(), configGroup()->readEntry( "AutoReply", "" ) );
	}
	// going online
	else
	{
		kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "Must be connected before changing status" << endl;
		m_initialReason = reason.message();
		connect( status );
	}
}

void GroupWiseAccount::setStatusMessage( const Kopete::StatusMessage & statusMessage )
{
	int currentStatus = myself()->onlineStatus().internalStatus();
	m_client->setStatus( ( GroupWise::Status )currentStatus, statusMessage.message(), configGroup()->readEntry( "AutoReply", "" ) );
}

void GroupWiseAccount::disconnect ()
{
	disconnect ( Manual );
}

void GroupWiseAccount::disconnect( Kopete::Account::DisconnectReason reason )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;

	if( isConnected () )
	{
		kDebug (GROUPWISE_DEBUG_GLOBAL) << k_funcinfo << "Still connected, closing connection..." << endl;
		/* Tell backend class to disconnect. */
		m_client->close ();
	}

	// clear the model of the server side contact list, so that when we reconnect, there will not be any stale entries to confuse GroupWiseContact::syncGroups()
	delete m_serverListModel;
	m_serverListModel = 0;

	// make sure that the connection animation gets stopped if we're still
	// in the process of connecting
	myself()->setOnlineStatus( GroupWiseProtocol::protocol()->groupwiseOffline );

	disconnected( reason );
	kDebug(GROUPWISE_DEBUG_GLOBAL) << k_funcinfo << "Disconnected." << endl;
}

void GroupWiseAccount::cleanup()
{
	delete m_client;
	delete m_clientStream;
	delete m_QCATLS;
	delete m_connector;

	m_connector = 0;
	m_QCATLS = 0;
	m_clientStream = 0;
	m_client = 0;
}

void GroupWiseAccount::createConference( const int clientId, const QStringList& invitees )
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	// TODO: remove this it prevents sending a list of participants with the createconf
	if ( isConnected() )
		m_client->createConference( clientId , invitees );
}

void GroupWiseAccount::sendInvitation( const GroupWise::ConferenceGuid & guid, const QString & dn, const QString & message )
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	if ( isConnected() )
	{
		GroupWise::OutgoingMessage msg;
		msg.guid = guid;
		msg.message = message;
		m_client->sendInvitation( guid, dn, msg );
	}
}

void GroupWiseAccount::slotLoggedIn()
{
	reconcileOfflineChanges();
	// set local status display
	myself()->setOnlineStatus( protocol()->groupwiseAvailable );
	// set status on server
	if ( initialStatus() != Kopete::OnlineStatus(Kopete::OnlineStatus::Online) &&
		( ( GroupWise::Status )initialStatus().internalStatus() != GroupWise::Unknown ) )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << "Initial status is not online, setting status to " << initialStatus().internalStatus() << endl;
		m_client->setStatus( ( GroupWise::Status )initialStatus().internalStatus(), m_initialReason, configGroup()->readEntry( "AutoReply", "" ) );
	}
}

void GroupWiseAccount::reconcileOfflineChanges()
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	m_dontSync = true;
	//sanity check the server side model vs our contact list.
	//Cont->acts might have been removed from some groups or entirely on the server.  
	//Any contact not present on the server should be deleted locally.

	// for each metacontact group membership:
	// for each GroupWiseContact
	//   get its contact list instances
	//   get its metacontact's groups
	//   for each group
	//    is there no CLI with the same id?
	//     if MC has no other contacts
	//       if MC's groups size is 1
	//         remove MC
	//       else
	//         remove from group
	//     else
	//       if MC's groups size is 1 and group is topLevel
	//         remove contact
	//       else  // Contact's group membership were changed elsewhere, but we can't change it here without 
	//             // affecting other protocols' contacts
	//       	set flag to warn user that incompatible changes were made on other client
	bool conflicts = false;
	QHashIterator<QString, Kopete::Contact*> it( contacts() );
	while( it.hasNext() )
	{
		it.next();
		if ( it.value() == myself() )
			continue;

		GroupWiseContact * c = static_cast< GroupWiseContact *>( it.value() );
		GWContactInstanceList instances = m_serverListModel->instancesWithDn( c->dn() );
		QListIterator<Kopete::Group *> grpIt( c->metaContact()->groups() );
		while ( grpIt.hasNext() )
		{
			Kopete::Group * grp = grpIt.next();
			bool found = false;
			QMutableListIterator<GWContactInstance*> instIt( instances );
			while ( instIt.hasNext() )
			{
				instIt.next();
				QString groupId = grp->pluginData( protocol(), accountId() + " objectId" );
				if ( groupId.isEmpty() )
					if ( grp == Kopete::Group::topLevel() )
						groupId = '0';	// hack the top level's objectId to 0
					else
						continue;

				GWFolder * folder = qobject_cast<GWFolder*>( instIt.value()->parent() );
				if ( folder->id == ( unsigned int )groupId.toInt() )
				{
					found = true;
					instIt.remove();
					break;
				}
			}
			if ( !found )
			{
				if ( c->metaContact()->contacts().count() == 1 )
				{
					if ( c->metaContact()->groups().count() == 1 )
					{
						kDebug( GROUPWISE_DEBUG_GLOBAL ) << "contact instance " << c->dn() << " not found on server side list, deleting metacontact with only this contact, in one group" << c->metaContact()->displayName() << endl;
						Kopete::ContactList::self()->removeMetaContact( c->metaContact() );
						break;
					}
					else
					{
						kDebug( GROUPWISE_DEBUG_GLOBAL ) << "contact instance " << c->dn() << " not found, removing metacontact " << c->metaContact()->displayName() << " from group " << grp->displayName() << endl;
						c->metaContact()->removeFromGroup( grp );
					}
				}
				else
				{
					if ( c->metaContact()->groups().count() == 1 )
					{
						kDebug( GROUPWISE_DEBUG_GLOBAL ) << "contact instance " << c->dn() << " not found, removing contact " << c->metaContact()->displayName() << " from metacontact with other contacts " << endl;
						c->deleteLater();
						break;
					}
					else
						kDebug( GROUPWISE_DEBUG_GLOBAL ) << "metacontact " << c->metaContact()->displayName( ) << "has multiple children and group membership, and contact " << c->dn() << " was removed from one group on the server." << endl;
						conflicts = true;
				}
			} // 
		} //end while, now check the next group membership
	} //end while, now check the next groupwise contact
	if ( conflicts )
		// show queuedmessagebox
		KPassivePopup::message( i18n( "Conflicting Changes Made Offline" ), i18n( "A change happened to your GroupWise contact list while you were offline which was impossible to reconcile." ), Kopete::UI::Global::mainWidget() );
	m_dontSync = false;
}

void GroupWiseAccount::slotLoginFailed()
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	password().setWrong();
	disconnect();
	connect();
}

void GroupWiseAccount::slotKopeteGroupRenamed( Kopete::Group * renamedGroup )
{
	if ( isConnected() )
	{
		QString objectIdString = renamedGroup->pluginData( protocol(), accountId() + " objectId" );
		// if this group exists on the server
		if ( !objectIdString.isEmpty() )
		{
			kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;

			GroupWise::FolderItem fi;
			fi.id = objectIdString.toInt();
			if ( fi.id != 0 )
			{
				fi.sequence = renamedGroup->pluginData( protocol(), accountId() + " sequence" ).toInt();
				fi.name= renamedGroup->pluginData( protocol(), accountId() + " serverDisplayName" );

				UpdateFolderTask * uft = new UpdateFolderTask( client()->rootTask() );
				uft->renameFolder( renamedGroup->displayName(), fi );
				uft->go( true );
				// would be safer to do this in a slot fired on uft's finished() signal
				renamedGroup->setPluginData( protocol(), accountId() + " serverDisplayName",
				renamedGroup->displayName() );
			}
		}
	}
	//else
	// errornotconnected
}

void GroupWiseAccount::slotKopeteGroupRemoved( Kopete::Group * group )
{
	if ( isConnected() )
	{
		kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
		// the member contacts should be deleted separately, so just delete the folder here
		// get the folder object id
		QString objectIdString = group->pluginData( protocol(), accountId() + " objectId" );
		if ( !objectIdString.isEmpty() )
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << "deleting folder with objectId: " << objectIdString << endl;
			int objectId = objectIdString.toInt();
			if ( objectId == 0 )
			{
				kDebug( GROUPWISE_DEBUG_GLOBAL ) << "deleted folder " << group->displayName() << " has root folder objectId 0!" << endl;
				return;
			}
			DeleteItemTask * dit = new DeleteItemTask( client()->rootTask() );
			dit->item( 0, objectId );
			// the group is deleted synchronously after this slot returns; so there is no point listening for signals
			dit->go( true );
		}
	}
	//else
	// errornotconnected
}

void GroupWiseAccount::slotConnError()
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget(), KMessageBox::Sorry,
				i18nc( "Error shown when connecting failed", "Kopete was not able to connect to the GroupWise Messenger server for account '%1'.\nPlease check your server and port settings and try again.", accountId() ) , i18n ("Unable to Connect '%1'", accountId() ) );

	disconnect();
}

void GroupWiseAccount::slotConnConnected()
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
}

void GroupWiseAccount::slotCSDisconnected()
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "Disconnected from Groupwise server." << endl;
	myself()->setOnlineStatus( protocol()->groupwiseOffline );
	setAllContactsStatus( protocol()->groupwiseOffline );
}

void GroupWiseAccount::slotCSConnected()
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "Connected to Groupwise server." << endl;

}

void GroupWiseAccount::slotCSError( int error )
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "Got error from ClientStream:" << error << endl;
}

void GroupWiseAccount::slotCSWarning( int warning )
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "Got warning from ClientStream:" << warning << endl;
}

void GroupWiseAccount::slotTLSHandshaken()
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "TLS handshake complete" << endl;
	QCA::TLS::IdentityResult identityResult = m_QCATLS->peerIdentityResult();
	QCA::Validity            validityResult = m_QCATLS->peerCertificateValidity();

	if( identityResult == QCA::TLS::Valid && validityResult == QCA::ValidityGood )
	{
		kDebug ( GROUPWISE_DEBUG_GLOBAL ) << "Certificate is valid, continuing." << endl;
		// valid certificate, continue
		m_tlsHandler->continueAfterHandshake ();
	}
	else
	{
		kDebug ( GROUPWISE_DEBUG_GLOBAL ) << "Certificate is not valid, continuing anyway" << endl;
		// certificate is not valid, query the user
		if(handleTLSWarning ( identityResult, validityResult, server(), myself()->contactId ()) == KMessageBox::Continue)
		{
			m_tlsHandler->continueAfterHandshake ();
		}
		else
		{
			disconnect ( Kopete::Account::Manual );
		}
	}
}

int GroupWiseAccount::handleTLSWarning ( QCA::TLS::IdentityResult identityResult,
		QCA::Validity validityResult , QString server, QString accountId)
{
	QString validityString, idString, code, idCode;

	switch ( identityResult )
	{
		case QCA::TLS::Valid:
			break;
		case QCA::TLS::HostMismatch:
			idString = i18n("The host name does not match the one in the certificate.");
			idCode   = "HostMismatch";
			break;
		case QCA::TLS::InvalidCertificate:
			idString = i18n("The certificate is invalid.");
			idCode   = "InvalidCert";
			break;
		case QCA::TLS::NoCertificate:
			idString = i18n("No certificate was presented.");
			idCode   = "NoCert";
			break;
	}

	switch ( validityResult )
	{
		case QCA::ValidityGood:
			break;
		case QCA::ErrorRejected:
			validityString = i18n("The Certificate Authority rejected the certificate.");
			code = "Rejected";
			break;
		case QCA::ErrorUntrusted:
			validityString = i18n("The certificate is not trusted.");
			code = "Untrusted";
			break;
		case QCA::ErrorSignatureFailed:
			validityString = i18n("The signature is invalid.");
			code = "SignatureFailed";
			break;
		case QCA::ErrorInvalidCA:
			validityString = i18n("The Certificate Authority is invalid.");
			code = "InvalidCA";
			break;
		case QCA::ErrorInvalidPurpose:
			validityString = i18n("Invalid certificate purpose.");
			code = "InvalidPurpose";
			break;
		case QCA::ErrorSelfSigned:
			validityString = i18n("The certificate is self-signed.");
			code = "SelfSigned";
			break;
		case QCA::ErrorRevoked:
			validityString = i18n("The certificate has been revoked.");
			code = "Revoked";
			break;
		case QCA::ErrorPathLengthExceeded:
			validityString = i18n("Maximum certificate chain length was exceeded.");
			code = "PathLengthExceeded";
			break;
		case QCA::ErrorExpired:
			validityString = i18n("The certificate has expired.");
			code = "Expired";
			break;
		case QCA::ErrorExpiredCA:
			validityString = i18n("The Certificate Authority has expired.");
			code = "ExpiredCA";
			break;
		case QCA::ErrorValidityUnknown:
			validityString = i18n("Validity is unknown.");
			code = "ValidityUnknown";
			break;
	}

	QString message;
   
	if (!idString.isEmpty())
	{
		if (!validityString.isEmpty())
		{
			message = i18n("<qt><p>The identity and the certificate of server %1 could not be "
					"validated for account %2:</p><p>%3</p><p>%4</p><p>Do you want to continue?</p></qt>",
					server, accountId, idString, validityString);
		}
		else
		{
			message = i18n("<qt><p>The certificate of server %1 could not be validated for "
					"account %2: %3</p><p>Do you want to continue?</p></qt>",
					server, accountId, idString);
		}
	} else {
		message = i18n("<qt><p>The certificate of server %1 could not be validated for "
			"account %2: %3</p><p>Do you want to continue?</p></qt>",
			server, accountId, validityString);
	}

	return ( KMessageBox::warningContinueCancel ( Kopete::UI::Global::mainWidget (),
					  message,
					  i18n("GroupWise Connection Certificate Problem"),
					  KStandardGuiItem::cont(),
					  QString("KopeteTLSWarning") + server + idCode + code) == KMessageBox::Continue );
}

void GroupWiseAccount::slotTLSReady( int secLayerCode )
{
	// i don't know what secLayerCode is for...
	Q_UNUSED( secLayerCode );
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	m_client->start( server(), port(), accountId(), password().cachedValue() );
}

void GroupWiseAccount::handleIncomingMessage( const ConferenceEvent & message )
{
	QString typeName = "UNKNOWN";
	if ( message.type == ReceiveMessage )
		typeName = "message";
	else if ( message.type == ReceiveAutoReply )
		typeName = "autoreply";
	else if ( message.type == ReceivedBroadcast )
		typeName = "broadcast";
	else if ( message.type == ReceivedSystemBroadcast )
		typeName = "system broadcast";

	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " received a " <<  typeName << " from " << message.user << ", to conference: " << message.guid << ", message: " << message.message << endl;

	GroupWiseContact * sender = contactForDN( message.user );
	if ( !sender )
		sender = createTemporaryContact( message.user );

	Kopete::ContactPtrList contactList;
	contactList.append( sender );
	// FIND A MESSAGE MANAGER FOR THIS CONTACT
	GroupWiseChatSession *sess = chatSession( contactList, message.guid, Kopete::Contact::CanCreate );

	// add an auto-reply indicator if needed
	QString messageMunged = message.message;
	if ( message.type == ReceiveAutoReply )
	{
		QString prefix = i18nc("Prefix used for automatically generated auto-reply"
			" messages when the contact is Away, contains contact's name",
			"Auto reply from %1: ", sender->metaContact()->displayName() );
		messageMunged = prefix + message.message;
	}
	if ( message.type == GroupWise::ReceivedBroadcast )
	{
		QString prefix = i18nc("Prefix used for broadcast messages",
			"Broadcast message from %1: ", sender->metaContact()->displayName() );
		messageMunged = prefix + message.message;
	}
	if ( message.type == GroupWise::ReceivedSystemBroadcast )
	{
		QString prefix = i18nc("Prefix used for system broadcast messages",
			"System Broadcast message from %1: ", sender->metaContact()->displayName() );
		messageMunged = prefix + message.message;
	}

	kDebug(GROUPWISE_DEBUG_GLOBAL) << k_funcinfo << " message before KopeteMessage and appending: " << messageMunged << endl;
	Kopete::Message * newMessage = 
			new Kopete::Message( message.timeStamp, sender, contactList, messageMunged,
								 Kopete::Message::Inbound, 
								 ( message.type == ReceiveAutoReply ) ? Kopete::Message::PlainText : Kopete::Message::RichText );
	Q_ASSERT( sess );
	sess->appendMessage( *newMessage );
	kDebug(GROUPWISE_DEBUG_GLOBAL) << "message from KopeteMessage: plainbody: " << newMessage->plainBody() << " parsedbody: " << newMessage->parsedBody() << endl;
	delete newMessage;
}

void GroupWiseAccount::receiveFolder( const FolderItem & folder )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo
			<< " objectId: " << folder.id
			<< " sequence: " << folder.sequence
			<< " parentId: " << folder.parentId
			<< " displayName: " << folder.name << endl;
	if ( folder.parentId != 0 )
	{
		kWarning( GROUPWISE_DEBUG_GLOBAL ) << " - received a nested folder.  These were not supported in GroupWise or Kopete as of Sept 2004, aborting! (parentId = " << folder.parentId << ')' << endl;
		return;
	}

	GWFolder * fld = m_serverListModel->addFolder( folder.id, folder.sequence, folder.name );
	Q_ASSERT( fld );

	// either find a local group and record these details there, or create a new group to suit
	Kopete::Group * found = 0;
	foreach( Kopete::Group * grp, Kopete::ContactList::self()->groups() )
	{
		// see if there is already a local group that matches this group
		QString groupId = grp->pluginData( protocol(), accountId() + " objectId" );
		if ( groupId.isEmpty() )
			if ( folder.name == grp->displayName() ) // no match on id, match on display name instead
			{
				grp->setPluginData( protocol(), accountId() + " objectId", QString::number( folder.id ) );
				found = grp;
				break;
			}
		if ( folder.id == (unsigned int)groupId.toInt() )
		{
			// was it renamed locally while we were offline?
			if ( grp->displayName() != folder.name )
			{
				slotKopeteGroupRenamed( grp );
				grp->setPluginData( protocol(), accountId() + " serverDisplayName", grp->displayName() );
				fld->displayName = grp->displayName();
			}

			found = grp;
			break;
		}
	}

	if ( !found )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " - not found locally, creating Kopete::Group" << endl;
		Kopete::Group * grp = new Kopete::Group( folder.name );
		grp->setPluginData( protocol(), accountId() + " serverDisplayName", folder.name );
		grp->setPluginData( protocol(), accountId() + " objectId", QString::number( folder.id ) );
		Kopete::ContactList::self()->addGroup( grp );
	}
}

void GroupWiseAccount::receiveContact( const ContactItem & contact )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo
			<< " objectId: " << contact.id
			<< ", sequence: " << contact.sequence
			<< ", parentId: " << contact.parentId
			<< ", dn: " << contact.dn
			<< ", displayName: " << contact.displayName << endl;
	//kDebug( GROUPWISE_DEBUG_GLOBAL ) << "\n dotted notation is '" << protocol()->dnToDotted( contact.dn ) << "'\n" <<endl;

	// add to new style contact list
	GWContactInstance * gwInst = m_serverListModel->addContactInstance( contact.id, contact.parentId, contact.sequence, contact.displayName, contact.dn );
	Q_ASSERT( gwInst );

	GroupWiseContact * c = contactForDN( contact.dn );
	// this contact is new to us, create him on the server
	if ( !c )
	{
		Kopete::MetaContact *metaContact = new Kopete::MetaContact();
		metaContact->setDisplayName( contact.displayName );
		c = new GroupWiseContact( this, contact.dn, metaContact, contact.id, contact.parentId, contact.sequence );
		Kopete::ContactList::self()->addMetaContact( metaContact );
	}
	// add the metacontact to the ContactItem's group, if not there aleady
	if ( contact.parentId == 0 )
		c->metaContact()->addToGroup( Kopete::Group::topLevel() );
	else
	{
		// check the metacontact is in the group this listing-of-the-contact is in...
		GWFolder * folder = m_serverListModel->findFolderById( contact.parentId );
		if ( !folder ) // inconsistent
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << " - ERROR - contact's folder doesn't exist on server" << endl;
			DeleteItemTask * dit = new DeleteItemTask( client()->rootTask() );
			dit->item( contact.parentId, contact.id );
//			QObject::connect( dit, SIGNAL( gotContactDeleted( const ContactItem & ) ), SLOT( receiveContactDeleted( const ContactItem & ) ) );
			dit->go( true );
			return;
		}
		Kopete::Group *grp = Kopete::ContactList::self()->findGroup( folder->displayName );
		// grp should exist, because we receive the folders from the server before the contacts
		if ( grp )
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << " - making sure MC is in group " << grp->displayName() << endl;
			m_dontSync = true;
			c->metaContact()->addToGroup( grp ); //addToGroup() is safe to call if already a member
			m_dontSync = false;
		}
	}

	c->setNickName( contact.displayName );
	//m_serverListModel->dump();
}

void GroupWiseAccount::receiveAccountDetails( const ContactDetails & details )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo
		<< "Auth attribute: " << details.authAttribute
		<< ", Away message: " << details.awayMessage
		<< ", CN" << details.cn
		<< ", DN" << details.dn
		<< ", fullName" << details.fullName
		<< ", surname" << details.surname
		<< ", givenname" << details.givenName
		<< ", status" << details.status
		<< endl;
	if ( details.cn.toLower() == accountId().toLower().section('@', 0, 0) ) // incase user set account ID foo@novell.com
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " - got our details in contact list, updating them" << endl;
		GroupWiseContact * detailsOwner= static_cast<GroupWiseContact *>( myself() );
		detailsOwner->updateDetails( details );
		//detailsOwner->setProperty( Kopete::Global::Properties::self()->nickName(), details.fullName );

		// Very important, without knowing our DN we can't do much else
		Q_ASSERT( !details.dn.isEmpty() );
		m_client->setUserDN( details.dn );
		return;
	}
	else
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " - passed someone else's details in contact list!" << endl;
	}
}

void GroupWiseAccount::receiveContactUserDetails( const ContactDetails & details )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo
		<< "Auth attribute: " << details.authAttribute
		<< ", Away message: " << details.awayMessage
		<< ", CN" << details.cn
		<< ", DN" << details.dn
		<< ", fullName" << details.fullName
		<< ", surname" << details.surname
		<< ", givenname" << details.givenName
		<< ", status" << details.status
		<< endl;
	// HACK: lowercased DN
	if ( !details.dn.isNull() )
	{
		// are the details for someone in our contact list?
		GroupWiseContact * detailsOwner = contactForDN( details.dn );

		if( detailsOwner )
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << " - updating details for " << details.dn << endl;
			detailsOwner->updateDetails( details );
		}
		else
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << " - got details for " << details.dn << ", but they aren't in our contact list!" << endl;
		}
	}
}

GroupWiseContact * GroupWiseAccount::createTemporaryContact( const QString & dn )
{
	ContactDetails details = client()->userDetailsManager()->details( dn );
	GroupWiseContact * c = static_cast<GroupWiseContact *>( contacts()[ details.dn.toLower() ] );
	if ( !c && details.dn != accountId() )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << "Got a temporary contact DN: " << details.dn << endl;
		// the client is telling us about a temporary contact we need to know about so add them
		Kopete::MetaContact *metaContact = new Kopete::MetaContact ();
		metaContact->setTemporary (true);
		QString displayName = details.fullName;
		if ( displayName.isEmpty() )
			displayName = details.givenName + ' ' + details.surname;

		metaContact->setDisplayName( displayName );
		c = new GroupWiseContact( this, details.dn, metaContact, 0, 0, 0 );
		c->updateDetails( details );
		c->setProperty( Kopete::Global::Properties::self()->nickName(), protocol()->dnToDotted( details.dn ) );
		Kopete::ContactList::self()->addMetaContact( metaContact );
		// the contact details probably don't contain status - but we can ask for it
		if ( details.status == GroupWise::Invalid && isConnected() )
			m_client->requestStatus( details.dn );
	}
	else
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << "Notified of existing temporary contact DN: " << details.dn << endl;
	return c;
}

void GroupWiseAccount::receiveStatus( const QString & contactId, quint16 status, const QString &awayMessage )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "got status for: " << contactId << ", status: " << status << ", away message: " << awayMessage << endl;
	GroupWiseContact * c = contactForDN( contactId );
	if ( c )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " - their KOS is: " << protocol()->gwStatusToKOS( status ).description() << endl;
		Kopete::OnlineStatus kos = protocol()->gwStatusToKOS( status );
		c->setOnlineStatus( kos );
		c->setProperty( protocol()->propAwayMessage, awayMessage );
	}
	else
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " couldn't find " << contactId << endl;
}

void GroupWiseAccount::changeOurStatus( GroupWise::Status status, const QString & awayMessage, const QString & autoReply )
{
	if ( status == GroupWise::Offline )
		myself()->setOnlineStatus( protocol()->groupwiseAppearOffline );
	else
		myself()->setOnlineStatus( protocol()->gwStatusToKOS( status ) );
	myself()->setProperty( protocol()->propAwayMessage, awayMessage );
	myself()->setProperty( protocol()->propAutoReply, autoReply );
}

void GroupWiseAccount::sendMessage( const GroupWise::ConferenceGuid &guid, const Kopete::Message & message )
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	// make an outgoing message
	if ( isConnected() )
	{
		GroupWise::OutgoingMessage outMsg;
		outMsg.guid = guid;
		outMsg.message = message.plainBody();
		outMsg.rtfMessage = protocol()->rtfizeText( message.plainBody() );
		// make a list of DNs to send to
		QStringList addresseeDNs;
		Kopete::ContactPtrList addressees = message.to();
		foreach( Kopete::Contact * contact, message.to() )
			addresseeDNs.append( static_cast< GroupWiseContact* >( contact )->dn() );
		// send the message
		m_client->sendMessage( addresseeDNs, outMsg );
	}
}

bool GroupWiseAccount::createContact( const QString& contactId, Kopete::MetaContact* parentContact )
{
	kDebug ( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "contactId: " << contactId << endl;

	// first find all the groups that this contact is a member of
	// record, in a folderitem, their display names and groupwise object id
	// Set object id to 0 if not found - they do not exist on the server
	bool topLevel = false;
	Q3ValueList< FolderItem > folders;
	foreach ( Kopete::Group *group, parentContact->groups() )
	{
		if ( group->type() == Kopete::Group::TopLevel ) // no need to create it on the server
		{
			topLevel = true;
			continue;
		}

		kDebug( GROUPWISE_DEBUG_GLOBAL ) << "looking up: " << group->displayName() << endl;
		GWFolder * fld = m_serverListModel->findFolderByName( group->displayName() );
		FolderItem fi;
		if ( fld )
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << fld->displayName << endl;
			//FIXME - get rid of FolderItem & co
			fi.parentId = qobject_cast<GWFolder*>( fld->parent() )->id;
			fi.id = fld->id;
			fi.name = fld->displayName;
		}
		else
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << "folder: " << group->displayName() << 
				"not found in server list model." << endl;
			fi.parentId = 0;
			fi.id = 0;
			fi.name = group->displayName();
		}
		folders.append( fi );
	}

	// find out the sequence number to use for any new folders
	int highestFreeSequence = m_serverListModel->maxSequenceNumber() + 1;

	// send this list along with the contact details to the server
	// CreateContactTask will create the missing folders on the server
	// and then add the contact to each one
	// finally it will signal finished(), and we can query it for the details
	// we gave it earlier and make sure the contact was successfully created.
	//
	// Since ToMetaContact expects synchronous contact creation
	// we have to create the contact optimistically.
	GroupWiseContact * gc = new GroupWiseContact( this, contactId, parentContact, 0, 0, 0 );
	ContactDetails dt = client()->userDetailsManager()->details( contactId );
	QString displayAs;
	if ( dt.fullName.isEmpty() )
		displayAs = dt.givenName + ' ' + dt.surname;
	else
		displayAs = dt.fullName;

	gc->setNickName( displayAs );
	// If the CreateContactTask finishes with an error, we have to
	// delete the contact we just created, in receiveContactCreated :/

	if ( folders.isEmpty() && !topLevel )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << "aborting because we didn't find any groups to add them to" << endl;
		return false;
	}
	
	// get the contact's full name to use as the display name of the created contact
	CreateContactTask * cct = new CreateContactTask( client()->rootTask() );
	cct->contactFromUserId( contactId, parentContact->displayName(), highestFreeSequence, folders, topLevel );
	QObject::connect( cct, SIGNAL( finished() ), SLOT( receiveContactCreated() ) );
	cct->go( true );
	return true;
}

void GroupWiseAccount::receiveContactCreated()
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	m_serverListModel->dump();

	CreateContactTask * cct = ( CreateContactTask * )sender();
	if ( cct->success() )
	{
		if ( client()->userDetailsManager()->known( cct->dn() ) )
		{
			ContactDetails dt = client()->userDetailsManager()->details( cct->dn() );
			GroupWiseContact * c = contactForDN( cct->dn() );
			c->setOnlineStatus( protocol()->gwStatusToKOS( dt.status ) );
			c->setNickName( dt.fullName );
			c->updateDetails( dt );
		}
		else
		{
			client()->requestDetails( QStringList( cct->dn() ) );
			client()->requestStatus( cct->dn() );
		}
	}
	else
	{
		// delete the contact created optimistically using the supplied userid;
		Kopete::Contact * c = contacts()[ protocol()->dnToDotted( cct->userId() ) ];
		if ( c )
		{
			if ( c->metaContact()->contacts().count() == 1 )
				Kopete::ContactList::self()->removeMetaContact( c->metaContact() );
			else	
				delete c;
		}

		KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget (), KMessageBox::Error,
							i18n ("The contact %1 could not be added to the contact list, with error message: %2", 
							cct->userId(), cct->statusString() ),
							i18n ("Error Adding Contact") );
	}
}

void GroupWiseAccount::deleteContact( GroupWiseContact * contact )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	contact->setDeleting( true );
	if ( isConnected() )
	{
		// remove all the instances of this contact from the server's contact list
		GWContactInstanceList instances = m_serverListModel->instancesWithDn( contact->dn() );
		GWContactInstanceList::iterator it = instances.begin();
		for ( ; it != instances.end(); ++it )
		{
			DeleteItemTask * dit = new DeleteItemTask( client()->rootTask() );
			dit->item( qobject_cast<GWFolder*>( (*it)->parent() )->id, (*it)->id );
			QObject::connect( dit, SIGNAL( gotContactDeleted( const ContactItem & ) ), SLOT( receiveContactDeleted( const ContactItem & ) ) );
			dit->go( true );
		}
	}
}

void GroupWiseAccount::receiveContactDeleted( const ContactItem & instance )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	// an instance of this contact was deleted on the server.
	// Remove it from the model of the server side list,
	// and if there are no other instances of this contact, delete the contact
	m_serverListModel->removeInstanceById( instance.id );
	m_serverListModel->dump();

	GWContactInstanceList instances = m_serverListModel->instancesWithDn( instance.dn );
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << " - " << instance.dn << " now has " << instances.count() << " instances remaining." << endl;
	GroupWiseContact * c = contactForDN( instance.dn );
	if ( c && instances.count() == 0 && c->deleting() )
	{
		c->deleteLater();
	}
}


void GroupWiseAccount::slotConnectedElsewhere()
{
	KPassivePopup::message( i18n ( "Signed in as %1 Elsewhere", accountId() ), i18nc(  "The parameter is the user's own account id for this protocol", "You have been disconnected from GroupWise Messenger because you signed in as %1 elsewhere", accountId() ) , Kopete::UI::Global::mainWidget() );
	disconnect();
}

void GroupWiseAccount::receiveInvitation( const ConferenceEvent & event )
{
	// ask the user if they want to accept the invitation or not
	GroupWiseContact * contactFrom = contactForDN( event.user );
	if ( !contactFrom )
		contactFrom = createTemporaryContact( event.user );
	if ( configGroup()->readEntry( "AlwaysAcceptInvitations",false ) == true )
	{
		client()->joinConference( event.guid );
	}
	else
	{
		ReceiveInvitationDialog * dlg = new ReceiveInvitationDialog( this, event,
				Kopete::UI::Global::mainWidget(), "invitedialog" );
		dlg->show();
	}

}

void GroupWiseAccount::receiveConferenceJoin( const GroupWise::ConferenceGuid & guid, const QStringList & participants, const QStringList & invitees )
{
	// get a new GWMM
	Kopete::ContactPtrList others;
	GroupWiseChatSession * sess = chatSession( others, guid, Kopete::Contact::CanCreate);
	// find each contact and add them to the GWMM, and tell them they are in the conference
	QStringListIterator joinerIt( participants );
	while ( joinerIt.hasNext() )
	{
		//kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " adding participant " << *it << endl;
		QString dn = joinerIt.next();
		GroupWiseContact * c = contactForDN( dn );
		if ( !c )
			c = createTemporaryContact( dn );
		sess->joined( c );	
	}
	// add each invitee too
	QStringListIterator inviteeIt( invitees );
	while ( inviteeIt.hasNext() )
	{
		//kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " adding invitee " << *it << endl;
		QString dn = inviteeIt.next();
		GroupWiseContact * c = contactForDN( dn );
		if ( !c )
			c = createTemporaryContact( dn );
		sess->addInvitee( c );
	}
	sess->view( true )->raise( false );
}

void GroupWiseAccount::receiveConferenceJoinNotify( const ConferenceEvent & event )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	GroupWiseChatSession * sess = findChatSessionByGuid( event.guid );
	if ( sess )
	{
		GroupWiseContact * c = contactForDN( event.user );
		if ( !c )
			c = createTemporaryContact( event.user );
		sess->joined( c );
	}
	else
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " couldn't find a GWCS for conference: " << event.guid << endl;
}

void GroupWiseAccount::receiveConferenceLeft( const ConferenceEvent & event )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	GroupWiseChatSession * sess = findChatSessionByGuid( event.guid );
	if ( sess )
	{
		GroupWiseContact * c = contactForDN( event.user );
		if ( c )
		{
			sess->left( c );
		}
		else
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " couldn't find a contact for DN: " << event.user << endl;
	}
	else
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " couldn't find a GWCS for conference: " << event.guid << endl;

}
	
void GroupWiseAccount::receiveInviteDeclined( const ConferenceEvent & event )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	GroupWiseChatSession * sess = findChatSessionByGuid( event.guid );
	if ( sess )
	{
		GroupWiseContact * c = contactForDN( event.user );
		if ( c )
			sess->inviteDeclined( c );
	}
	else
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " couldn't find a GWCS for conference: " << event.guid << endl;
}

void GroupWiseAccount::receiveInviteNotify( const ConferenceEvent & event )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
	GroupWiseChatSession * sess = findChatSessionByGuid( event.guid );
	if ( sess )
	{
		GroupWiseContact * c = contactForDN( event.user );
		if ( !c )
			c = createTemporaryContact( event.user );

		sess->addInvitee( c );
		Kopete::Message declined = Kopete::Message( myself(), sess->members(), i18n("%1 has been invited to join this conversation.", c->metaContact()->displayName() ), Kopete::Message::Internal, Kopete::Message::PlainText );
		sess->appendMessage( declined );
	}
	else
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " couldn't find a GWCS for conference: " << event.guid << endl;
}

void GroupWiseAccount::slotLeavingConference( GroupWiseChatSession * sess )
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "unregistering message manager:" << sess->guid()<< endl;
	if( isConnected () )
		m_client->leaveConference( sess->guid() );
	m_chatSessions.remove( sess );
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << "m_chatSessions now contains:" << m_chatSessions.count() << " managers" << endl;
}

void GroupWiseAccount::slotSetAutoReply()
{
	bool ok;
	QRegExp rx( ".*" );
    QRegExpValidator validator( rx, this );
	QString newAutoReply = KInputDialog::getText( i18n( "Enter Auto-Reply Message" ),
			 i18n( "Please enter an Auto-Reply message that will be shown to users who message you while Away or Busy" ), configGroup()->readEntry( "AutoReply", "" ),
			 &ok, Kopete::UI::Global::mainWidget(), &validator );
	if ( ok )
		configGroup()->writeEntry( "AutoReply", newAutoReply );
}

void GroupWiseAccount::slotTestRTFize()
{
/*	bool ok;
	const QString query = QString::fromLatin1("Enter a string to rtfize:");
	QString testText = KLineEditDlg::getText( query, QString::null, &ok, Kopete::UI::Global::mainWidget() );
	if ( ok )
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << "Converted text is: '" << protocol()->rtfizeText( testText ) << "'" << endl;*/

// 	bool ok;
// 	const QString query = i18n("Enter a contactId:");
// 	QString testText = KInputDialog::getText( query, i18n("This is a test dialog and will not be in the final product!" ), QString::null, &ok, Kopete::UI::Global::mainWidget() );
// 	if ( !ok )
// 		return;
// 	kDebug( GROUPWISE_DEBUG_GLOBAL ) << "Trying to add contact: '" << protocol()->rtfizeText( testText ) << "'" << endl;
// 	Kopete::MetaContact *metaContact = new Kopete::MetaContact ();
// 	metaContact->setDisplayName( "Test Add MC" );
// 	metaContact->setTemporary (true);
// 	createContact( testText, "Test Add Contact", metaContact );
}

void GroupWiseAccount::slotPrivacy()
{
	new GroupWisePrivacyDialog( this, Kopete::UI::Global::mainWidget(), "gwprivacydialog" );
}

void GroupWiseAccount::slotJoinChatRoom()
{
	new GroupWiseChatSearchDialog( this, Kopete::UI::Global::mainWidget(), "gwjoinchatdialog" );
}

bool GroupWiseAccount::isContactBlocked( const QString & dn )
{
	if ( isConnected() )
		return client()->privacyManager()->isBlocked( dn );
	else
		return false;
}

void GroupWiseAccount::dumpManagers()
{
	kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << " for: " << accountId()
		<< " containing: " << m_chatSessions.count() << " managers " << endl;
	Q3ValueList<GroupWiseChatSession *>::ConstIterator it;
	for ( it = m_chatSessions.begin() ; it != m_chatSessions.end(); ++it )
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << "guid: " << (*it)->guid() << endl;
}

bool GroupWiseAccount::dontSync()
{
	return m_dontSync;
}

void GroupWiseAccount::syncContact( GroupWiseContact * contact )
{
	if ( dontSync() )
		return;
	
	if ( contact != myself() )
	{
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << k_funcinfo << endl;
		if ( !isConnected() )
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << "not connected, can't sync display name or group membership" << endl;
			return;
		}
	
		// if this is a temporary contact, don't bother
		if ( contact->metaContact()->isTemporary() )
			return;

		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " = CONTACT '" << contact->nickName() << "' IS IN " << contact->metaContact()->groups().count() << " MC GROUPS, AND HAS " << m_serverListModel->instancesWithDn( contact->dn() ).count() << " CONTACT LIST INSTANCES." << endl;

		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " = LOOKING FOR NOOP GROUP MEMBERSHIPS" << endl;
		// 1) Seek matches between CLIs and MCGs and remove from the lists without taking any action. match on objectid, parentid
		// 2) Each remaining unmatched pair is a move, initiate and remove - need to take care to always use greatest unused sequence number - if we have to set the sequence number to the following sequence number within the folder, we may have a problem where after the first move, we have to wait for the state of the CLIs to be updated pending the completion of the first move - this would be difficult to cope with, because our current lists would be out of date, or we'd have to restart the sync - assuming the first move created a new matched CLI-MCG pair, we could do that with little cost.
		// 3) Any remaining entries in MCG list are adds, carry out
		// 4) Any remaining entries in CLI list are removes, carry out

		// start by discovering the next free group sequence number in case we have to add any groups
		int nextFreeSequence = m_serverListModel->maxSequenceNumber() + 1;
		// 1)
		// make a list of all the groups the metacontact is in
		QList<Kopete::Group *> groupList = contact->metaContact()->groups();
		// make a list of all the groups this contact is in, according to the server model
		GWContactInstanceList instances = m_serverListModel->instancesWithDn( contact->dn() );

		// seek corresponding pairs in both lists and remove
		// ( for each group )
		QMutableListIterator< Kopete::Group *> grpIt( groupList );
		while ( grpIt.hasNext() )
		{
			grpIt.next();
			
			QMutableListIterator< GWContactInstance *> instIt( instances );
			// ( see if a contactlist instance matches the group)
			while ( instIt.hasNext() )
			{
				instIt.next();
				GWFolder * folder = qobject_cast<GWFolder *>( instIt.value()->parent() );
				kDebug( GROUPWISE_DEBUG_GLOBAL ) << "  - Looking for a match, MC grp '" 
					<< grpIt.value()->displayName()
					<< "', GWFolder '" << folder->displayName << "', objectId is " << folder->id << endl;
				
				if ( ( folder->id == 0 && ( grpIt.value() == Kopete::Group::topLevel() ) )
						|| ( grpIt.value()->displayName() == folder->displayName ) )
				{
					//this pair matches, we can remove its members from both lists )
					kDebug( GROUPWISE_DEBUG_GLOBAL ) << "  - match! removing both entries" << endl;
					instIt.remove();
					grpIt.remove();
					break;
				}
			}
		}
		
		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " = LOOKING FOR UNMATCHED PAIRS => GROUP MOVES" << endl;
		grpIt.toFront();
		// ( take the first pair and carry out a move )
		while ( grpIt.hasNext() && !instances.isEmpty() )
		{
			grpIt.next();
			GWContactInstance * cliInstance = instances.takeFirst();
			GWFolder * sourceFolder = qobject_cast<GWFolder*>( cliInstance->parent() );
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << "  - moving contact instance from group '" << sourceFolder->displayName << "' to group '" << grpIt.value()->displayName() << "'" << endl;

			// create contactItem parameter
			ContactItem instance;
			instance.id = cliInstance->id;
			instance.parentId = sourceFolder->id;
			instance.sequence = cliInstance->sequence;
			instance.dn = cliInstance->dn;
			instance.displayName = contact->nickName();
			// identify the destination folder
			GWFolder * destinationFolder = m_serverListModel->findFolderByName( grpIt.value()->displayName() );
			if ( destinationFolder ) // folder already exists on the server
			{
				MoveContactTask * mit = new MoveContactTask( client()->rootTask() );
				mit->moveContact( instance, destinationFolder->id );
				QObject::connect( mit, SIGNAL( gotContactDeleted( const ContactItem & ) ), SLOT( receiveContactDeleted( const ContactItem & ) ) );
				mit->go();
			}
			else if ( grpIt.value() == Kopete::Group::topLevel() )
			{
				MoveContactTask * mit = new MoveContactTask( client()->rootTask() );
				mit->moveContact( instance, 0 );
				QObject::connect( mit, SIGNAL( gotContactDeleted( const ContactItem & ) ), SLOT( receiveContactDeleted( const ContactItem & ) ) );
				mit->go();
			}
			else
			{
				MoveContactTask * mit = new MoveContactTask( client()->rootTask() );
				QObject::connect( mit, SIGNAL( gotContactDeleted( const ContactItem & ) ), 
								  SLOT( receiveContactDeleted( const ContactItem & ) ) );
				// discover the next free sequence number and add the group using that
				mit->moveContactToNewFolder( instance, nextFreeSequence++,
											 grpIt.value()->displayName() );
				mit->go( true );
			}
			grpIt.remove();
		}

		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " = LOOKING FOR ADDS" << endl;
		grpIt.toFront();
		while ( grpIt.hasNext() )
		{
			grpIt.next();
			GWFolder * destinationFolder = m_serverListModel->findFolderByName( grpIt.value()->displayName() );
			CreateContactInstanceTask * ccit = new CreateContactInstanceTask( client()->rootTask() );

			contact->setNickName( contact->metaContact()->displayName() );
			// does this group exist on the server?  Create the contact appropriately
			if ( destinationFolder )
			{
				int parentId = destinationFolder->id;
				ccit->contactFromUserId( contact->dn(), contact->metaContact()->displayName(), parentId );
			}
			else
			{
				if ( grpIt.value() == Kopete::Group::topLevel() )
					ccit->contactFromUserId( contact->dn(), contact->metaContact()->displayName(),
							m_serverListModel->rootFolder->id );
				else
					// discover the next free sequence number and add the group using that
					ccit->contactFromUserIdAndFolder( contact->dn(), contact->metaContact()->displayName(),
							nextFreeSequence++, grpIt.value()->displayName() );
			}
			ccit->go( true );
			grpIt.remove();
		}

		kDebug( GROUPWISE_DEBUG_GLOBAL ) << " = LOOKING FOR REMOVES" << endl;
		QMutableListIterator<GWContactInstance *>  instIt( instances );
		// ( remove each remaining contactlist instance, because it doesn't exist locally any more )
		while ( instIt.hasNext() )
		{
			instIt.next();
			GWFolder * folder =qobject_cast<GWFolder*>( ( instIt.value() )->parent() );
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << "  - remove contact instance '"<< ( instIt.value() )->id << "' in group '" << folder->displayName << "'" << endl;

			DeleteItemTask * dit = new DeleteItemTask( client()->rootTask() );
			dit->item( folder->id, instIt.value()->id );
			QObject::connect( dit, SIGNAL( gotContactDeleted( const ContactItem & ) ), SLOT( receiveContactDeleted( const ContactItem & ) ) );
			dit->go( true );

			instIt.remove();
		}

		// start an UpdateItem
		if ( contact->metaContact()->displayName() != contact->nickName() )
		{
			kDebug( GROUPWISE_DEBUG_GLOBAL ) << " updating the contact's display name to the metacontact's: " << contact->metaContact()->displayName() << endl;
			foreach ( GWContactInstance * instance, m_serverListModel->instancesWithDn( contact->dn() ) )
			{
				QList< ContactItem > instancesToChange;
				ContactItem item;
				item.id = instance->id;
				item.parentId = qobject_cast<GWFolder *>( instance->parent() )->id;
				item.sequence = instance->sequence;
				item.dn = contact->dn();
				item.displayName = contact->nickName();
				instancesToChange.append( item );

				UpdateContactTask * uct = new UpdateContactTask( client()->rootTask() );
				uct->renameContact( contact->metaContact()->displayName(), instancesToChange );
				QObject::connect ( uct, SIGNAL( finished() ), contact, SLOT( renamedOnServer() ) );
				uct->go( true );
			}
		}
	}
}

#include "gwaccount.moc"
