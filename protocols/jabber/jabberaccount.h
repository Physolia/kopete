
/***************************************************************************
                jabberaccount.h  -  core Jabber account class
                             -------------------
    begin                : Sat Mar 8 2003
    copyright            : (C) 2003 by Till Gerken <till@tantalo.net>
							Based on JabberProtocol by Daniel Stone <dstone@kde.org>
							and Till Gerken <till@tantalo.net>.

			   Kopete (C) 2001-2003 Kopete developers
			   <kopete-devel@kde.org>.
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef JABBERACCOUNT_H
#define JABBERACCOUNT_H

// include these because of namespace reasons
#include <im.h>
#include <xmpp.h>
#include <s5b.h>

// we need these for type reasons
#include <kopetepasswordedaccount.h>
#include <kopeteonlinestatus.h>

class JabberConnector;
class JabberProtocol;
class QString;
class QStringList;
namespace Kopete { class MetaContact; }
class KActionMenu;
class JabberResourcePool;
class JabberContact;
class JabberContactPool;

#define JABBER_PENALTY_TIME	3

using namespace XMPP;

/* @author Daniel Stone, Till Gerken */

class JabberAccount : public Kopete::PasswordedAccount
{
	Q_OBJECT

public:
	  JabberAccount (JabberProtocol * parent, const QString & accountID, const char *name = 0L);
	/* Standard null destructor. */
	 ~JabberAccount ();

	/* Returns the action menu for this account. */
	virtual KActionMenu *actionMenu ();

	virtual void setAway (bool away, const QString & reason = QString::null);

	/* Return the resource of the client */
	const QString resource () const;
	const QString server () const;
	const int port () const;

	JabberResourcePool *resourcePool ();
	JabberContactPool *contactPool ();

	XMPP::Client *client();

	/* to get the protocol from the account */
	JabberProtocol *protocol () const
	{
		return mProtocol;
	}

	/**
	 * Returns if a connection attempt is currently in progress.
	 */
	bool isConnecting ();

	/* Tells the user to connect first before they can do whatever it is
	 * that they want to do. */
	void errorConnectFirst ();

	/* Tells the user that the connection was lost while we waited for
	 * an answer of him. */
	void errorConnectionLost ();

	/* Tells the user that a connection attempt is already in progress */
	void errorConnectionInProgress ();

	/*
	 * Handle TLS warnings. Displays a dialog and returns the user's choice.
	 * Parameters: Warning code from QCA::TLS, server name, account ID
	 * Returns KMessageBox::ButtonCode
	 */
	static int handleTLSWarning (int warning, QString server, QString accountId);

	/*
	 * Handle stream errors. Displays a dialog and returns.
	 */
	static void handleStreamError (int streamError, int streamCondition, int connectorCode, QString server, Kopete::Account::DisconnectReason &errorClass);

	/**
	 * Return the current, shared S5B server instance.
	 */
	XMPP::S5BServer *s5bServer ();
	void addS5bAddress ( const QString &address );
	void removeS5bAddress ( const QString &address );
	void setS5bPort ( int port );

	/**
	 * As the Jabber servers shouldn't be bombarded with
	 * requests, time intensive queries and mass queries
	 * should be slowed down to reasonable intervals.
	 * The method here offers a penalty service that
	 * returns a time in seconds a query should be
	 * delayed in order to not flood the server.
	 * Current hard-coded interval is 3 seconds.
	 */
	int getPenaltyTime ();

public slots:
	/* Connects to the server. */
	void connectWithPassword ( const QString &password );

	/* Disconnects from the server. */
	void disconnect ();

	/* Disconnect with a reason */
	void disconnect ( Kopete::Account::DisconnectReason reason );

protected:
	/**
	 * Create a new contact in the specified metacontact
	 *
	 * You shouldn't ever call this method yourself, For adding contacts see @ref addContact()
	 *
	 * This method is called by @ref Kopete::Account::addContact() in this method, you should
	 * simply create the new custom @ref Kopete::Contact in the given metacontact. You should
	 * NOT add the contact to the server here as this method gets only called when synchronizing
	 * the contact list on disk with the one in memory. As such, all created contacts from this
	 * method should have the "dirty" flag set.
	 *
	 * This method should simply be used to intantiate the new contact, everything else
	 * (updating the GUI, parenting to meta contact, etc.) is being taken care of.
	 *
	 * @param contactId The unique ID for this protocol
	 * @param displayName The displayname of the contact (may equal userId for some protocols)
	 * @param parentContact The metacontact to add this contact to
	 */
	virtual bool addContactToMetaContact (const QString & contactID, const QString & displayName, Kopete::MetaContact * parentContact);

private:
	/* Psi backend for this account. */
	XMPP::Client *jabberClient;
	XMPP::ClientStream *jabberClientStream;
	JabberConnector *jabberClientConnector;
	QCA::TLS *jabberTLS;
	XMPP::QCATLSHandler *jabberTLSHandler;

	JabberResourcePool *mResourcePool;
	JabberContactPool *mContactPool;

	QString localAddress;

	/*
	 * Current penalty time
	 */
	int mCurrentPenaltyTime;

	void setAvailable ();

	/* Set up our actions for the status menu. */
	void initActions ();

	void cleanup ();

	JabberProtocol *mProtocol;

	/* Initial presence to set after connecting. */
	XMPP::Status initialPresence;

	/* Caches the title ID of the account context menu. */
	int menuTitleId;

	static XMPP::S5BServer *m_s5bServer;
	static QStringList m_s5bAddressList;

	/**
	 * Sets our own presence. Updates our resource in the
	 * resource pool and sends a presence packet to the server.
	 */
	void setPresence ( const XMPP::Status &status );

private slots:
		/* Connects to the server. */
	void slotConnect ();

	void slotGoOffline ();

	/* Disconnects from the server. */
	void slotDisconnect ();

	/* Login if the connection was OK. */
	void slotCSNeedAuthParams (bool user, bool pass, bool realm);

	/* Called from Psi: tells us when we're logged in OK. */
	void slotCSAuthenticated ();

	/* Called from Psi: tells us when we've been disconnected from the server. */
	void slotCSDisconnected ();

	/* Called from Psi: alerts us to a protocol warning. */
	void slotCSWarning (int);

	/* Called from Psi: alerts us to a protocol error. */
	void slotCSError (int);

	/* Called from Psi: report certificate status */
	void slotTLSHandshaken ();

	/* Called from Psi: roster request finished */
	void slotRosterRequestFinished ( bool success, int statusCode, const QString &statusString );

	/* Called from Psi: incoming file transfer */
	void slotIncomingFileTransfer ();

	/* Called from Psi: debug messages from the backend. */
	void slotPsiDebug (const QString & msg);

	/* Set online mode (presence-wise, and connection-wise). */
	void slotGoOnline ();

	/* Set global presence to "free for chat", no reason.. */
	void slotGoChatty ();

	/* Set global presence to "away". */
	void slotGoAway ( const QString & );

	/* Set global presence to "extended away". */
	void slotGoXA ( const QString & );

	/* Set global presence to "do not disturb". */
	void slotGoDND ( const QString & );

	/* Set global presence to "invisible", no reason. */
	void slotGoInvisible ();

	/* Sends a raw message to the server (use with caution) */
	void slotSendRaw ();

	/* Slots for handling group chats. */
	void slotJoinNewChat ();
	void slotGroupChatJoined (const Jid & jid);
	void slotGroupChatLeft (const Jid & jid);
	void slotGroupChatPresence (const Jid & jid, const Status & status);
	void slotGroupChatError (const Jid & jid, int error, const QString & reason);

	/* Incoming subscription request. */
	void slotSubscription (const Jid & jid, const QString & type);

	/**
	 * A new item appeared in our roster, synch it with the
	 * contact list.
	 */
	void slotNewContact (const RosterItem &);

	/**
	 * An item has been deleted from our roster,
	 * delete it from our contact pool.
	 */
	void slotContactDeleted (const RosterItem &);

	/* Update a contact's details. */
	void slotContactUpdated (const RosterItem &);

	/* Someone on our contact list had (another) resource come online. */
	void slotResourceAvailable (const Jid &, const Resource &);

	/* Someone on our contact list had (another) resource go offline. */
	void slotResourceUnavailable (const Jid &, const Resource &);

	/* Displays a new message. */
	void slotReceivedMessage (const Message &);

	/* Gets the user's vCard from the server for editing. */
	void slotEditVCard ();

	/* Get the services list from the server for management. */
	void slotGetServices ();

	void slotS5bServerGone ();

	/*
	 * Update penalty timer
	 */
	void slotUpdatePenaltyTime ();

};

#endif
