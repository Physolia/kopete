
/*
    jabberprotocol.cpp  -  Base class for the Kopete Jabber protocol

    Copyright (c) 2002 by Daniel Stone <dstone@kde.org>
    Copyright (c) 2002 by Till Gerken <till@tantalo.net>

    Kopete    (c) 2002 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include <kdebug.h>
#include <kgenericfactory.h>
#include <kconfig.h>
#include <kmessagebox.h>
#include <kiconloader.h>
#include <kpopupmenu.h>
#include <kstandarddirs.h>
#include <klineeditdlg.h>

#include <qapplication.h>
#include <qcursor.h>
#include <qmap.h>
#include <qtimer.h>
#include <qpixmap.h>
#include <qstringlist.h>

#include "client.h"
#include "stream.h"
#include "tasks.h"
#include "types.h"
#include "vcard.h"

#include <sys/utsname.h>

#include "../kopete/kopete.h"
#include "kopetecontact.h"
#include "kopetecontactlist.h"
#include "kopetemetacontact.h"
#include "kopetemessagemanager.h"
#include "kopeteaway.h"
#include "kopeteprotocol.h"
#include "kopeteplugin.h"
#include "addcontactpage.h"
#include "jabbercontact.h"
//#include "jabberprefs.h"
#include "jabberdefprefs.h"
#include "dlgjabberstatus.h"
#include "dlgjabbersendraw.h"
#include "dlgjabberservices.h"
#include "dlgjabberchatjoin.h"
#include "jabberaddcontactpage.h"
#include "jabbermap.h"
#include "jabbergroupchat.h"
#include "jabberprotocol.h"
#include "jabberaccount.h"
#include "kopeteaccountmanager.h"
#include "jabbereditaccountwidget.h"
#include "kopetecommandhandler.h"


JabberProtocol *JabberProtocol::protocolInstance = 0;

KopeteOnlineStatus JabberProtocol::JabberOnline;
KopeteOnlineStatus JabberProtocol::JabberChatty;
KopeteOnlineStatus JabberProtocol::JabberAway;
KopeteOnlineStatus JabberProtocol::JabberXA;
KopeteOnlineStatus JabberProtocol::JabberDND;
KopeteOnlineStatus JabberProtocol::JabberOffline;
KopeteOnlineStatus JabberProtocol::JabberInvisible;

K_EXPORT_COMPONENT_FACTORY (kopete_jabber, KGenericFactory < JabberProtocol >);

JabberProtocol::JabberProtocol (QObject * parent, QString name, QStringList):KopeteProtocol (parent, name)
{

	JabberOnline = KopeteOnlineStatus (KopeteOnlineStatus::Online, 25, this, 0, "jabber_online", i18n ("Go O&nline"), i18n ("Online"));

	JabberChatty = KopeteOnlineStatus (KopeteOnlineStatus::Online, 20, this, 1, "jabber_chatty", i18n ("Set F&ree to Chat"), i18n ("Free to Chat"));

	JabberAway = KopeteOnlineStatus (KopeteOnlineStatus::Away, 25, this, 2, "jabber_away", i18n ("Set A&way"), i18n ("Away"));
	JabberXA = KopeteOnlineStatus (KopeteOnlineStatus::Away, 20, this, 3, "jabber_away", i18n ("Set E&xtended Away"), i18n ("Extended Away"));

	JabberDND = KopeteOnlineStatus (KopeteOnlineStatus::Away, 15, this, 4, "jabber_na", i18n ("Set &Do not Disturb"), i18n ("Do not Disturb"));

	JabberOffline = KopeteOnlineStatus (KopeteOnlineStatus::Offline, 20, this, 5, "jabber_offline", i18n ("Go O&ffline"), i18n ("Offline"));

	JabberInvisible = KopeteOnlineStatus (KopeteOnlineStatus::Online, 5, this, 6, "jabber_offline", i18n ("Set I&nvisible"), i18n ("Invisible"));

	kdDebug (JABBER_DEBUG_GLOBAL) << "[JabberProtocol] Loading ..." << endl;

	/* This is meant to be a singleton, so we will check if we have
	 * been loaded before. */
	if (protocolInstance)
	  {
		  kdDebug (JABBER_DEBUG_GLOBAL) << "[JabberProtocol] Warning: Protocol already " << "loaded, not initializing again." << endl;
		  return;
	  }

	protocolInstance = this;

	preferences = new JabberDefaultPreferences ("jabber_protocol", this);

	QObject::connect (preferences, SIGNAL (saved ()), this, SLOT (slotSettingsChanged ()));

	// read the Jabber ID from Kopete's configuration
	KGlobal::config ()->setGroup ("Jabber");

	// setup actions
	initActions ();

	// read remaining settings from configuration file
	slotSettingsChanged ();
	addAddressBookField ("messaging/xmpp", KopetePlugin::MakeIndexField);
}

JabberProtocol::~JabberProtocol ()
{
	//disconnectAll();

	/* make sure that the next attempt to load Jabber
	 * re-initializes the protocol class. */
	protocolInstance = 0L;
}



AddContactPage *JabberProtocol::createAddContactWidget (QWidget * parent, KopeteAccount * i)
{
	kdDebug (JABBER_DEBUG_GLOBAL) << "[Jabber Protocol] Create Add Contact  Widget\n";
	return new JabberAddContactPage (i, parent);
}

EditAccountWidget *JabberProtocol::createEditAccountWidget (KopeteAccount * account, QWidget * parent)
{
	kdDebug (JABBER_DEBUG_GLOBAL) << "[Jabber Protocol] Edit Account Widget\n";
	return new JabberEditAccountWidget (this, static_cast < JabberAccount * >(account), parent);
}

KopeteAccount *JabberProtocol::createNewAccount (const QString & accountId)
{
	kdDebug (JABBER_DEBUG_GLOBAL) << "[Jabber Protocol] Create New Account. ID: " << accountId << "\n";
	return new JabberAccount (this, accountId);
}


void JabberProtocol::errorConnectFirst ()
{
	KMessageBox::error (qApp->mainWidget (), i18n ("Please connect first"), i18n ("Error"));
}

KActionMenu *JabberProtocol::protocolActions ()
{
	KActionMenu *protocolMenu = new KActionMenu ();

	/*QDict<KopeteAccount> accounts = KopeteAccountManager::manager()->accounts( this );
	   for (JabberAccount * tmpAccount = accounts.first();
	   tmpAccount != accounts.last(); tmpAccount = accounts.next())
	   protocolMenu->insert(tmpAccount->actionMenu());
	 */
	return protocolMenu;
}

void JabberProtocol::initActions ()
{
	// initialize icon that sits in Kopete's status bar
	//setStatusIcon("jabber_offline");

	KGlobal::config ()->setGroup ("Jabber");
}

void JabberProtocol::connectAll ()
{
	kdDebug (JABBER_DEBUG_GLOBAL) << "[JabberProtocol] connectAll()" << endl;
	// QDict<KopeteAccount> accounts = KopeteAccountManager::manager()->accounts( this );
	//for (JabberAccount *tmpAccount = accounts.first(); tmpAccount; tmpAccount++) 
	//if (!accounts.isEmpty())
	//{
	//  JabberAccount * tmpAccount = accounts[0];
//  tmpAccount->connect();
	//   }
}

JabberProtocol *JabberProtocol::protocol ()
{
	// return current instance
	return protocolInstance;
}

void JabberProtocol::disconnectAll ()
{
	kdDebug (JABBER_DEBUG_GLOBAL) << "[JabberProtocol] disconnectAll()" << endl;
	/*
	   for (JabberAccount *tmpAccount = accounts.first(); tmpAccount; tmpAccount++)
	   tmpAccount->disconnect();
	 */
}

/*
 * Set presence (usually called by dialog widget)
 */
void JabberProtocol::setPresenceAll (const KopeteOnlineStatus & status, const QString & reason, int priority)
{
	/*for (JabberAccount *tmpAccount = accounts.first(); tmpAccount; tmpAccount++)
	   tmpAccount->setPresence(status, reason, priority);
	 */
}


void JabberProtocol::slotSettingsChanged ()
{
	KGlobal::config ()->setGroup ("Jabber");

	QString userId = KGlobal::config ()->readEntry ("UserID", "");
	QString server = KGlobal::config ()->readEntry ("Server", "jabber.org");

	// set the title according to the current account
	//actionStatusMenu->popupMenu()->changeTitle( menuTitleId , userId + "@" + server );

}

//void JabberProtocol::setAvailableAll(void) {
 //   setPresenceAll(JabberOnline);
//}

//KopeteContact *JabberProtocol::myself() const {
 //   return myContact;
//}

void JabberProtocol::deserializeContact (KopeteMetaContact * metaContact,
										 const QMap < QString, QString > &serializedData, const QMap < QString, QString > & /* addressBookData */ )
{
	kdDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "Deserializing data for metacontact " << metaContact->displayName () << "\n" << endl;

	QString contactId = serializedData["contactId"];
	QString displayName = serializedData["displayName"];

	kdDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "ContactId: " << contactId << "\n" << endl;

	if (displayName.isEmpty ())
		displayName = contactId;

	QDict < KopeteAccount > accounts = KopeteAccountManager::manager ()->accounts (this);
	if (!accounts.isEmpty ())
	  {
		  KopeteAccount *a = accounts[serializedData["accountId"]];

		  if (a)
			  a->addContact (contactId, displayName, metaContact);
		  else
			  kdDebug (14120) << k_funcinfo << serializedData["accountId"] << " was a contact's account," " but we dont have it in the accounts list" << endl;
	  }
	else
		kdDebug (14120) << k_funcinfo << "No accounts loaded!" << endl;

}

#include "jabberprotocol.moc"
