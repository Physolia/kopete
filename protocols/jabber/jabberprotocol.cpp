/***************************************************************************
                          jabberprotocol.cpp  -  Jabber Plugin
                             -------------------
    begin                : Fri Apr 12 2002
    copyright            : (C) 2002 by Daniel Stone
    email                : dstone@kde.org
 ***************************************************************************

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *                                                                         *
 *   Kudos to my lappy for letting me hack on the way to Sunbury, and CBV  *
 *   for lending it to me. :)                                              *
 *                                                                         *
 *   All this code has been written on a 120-odd character terminal, and   *
 *   thus the wrapping is changed accordingly; in fact, there is none.     *
 *                                                                         *
 *   Deal.                                                                 *
 *																		   *
 ***************************************************************************/

#include <qcursor.h>
#include <qmap.h>
#include <qtimer.h>

#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <ksimpleconfig.h>
#include <kstandarddirs.h>
#include <kgenericfactory.h>

#include "kopete.h"
#include "kopetemetacontact.h"
#include "kopetecontactlist.h"

#include "jabberprotocol.h"
#include "jabcommon.h"
#include "statusbaricon.h"
#include "dlgjabberstatus.h"

const JabberProtocol * JabberProtocol::sProtocol = 0;

K_EXPORT_COMPONENT_FACTORY(kopete_jabber, KGenericFactory<JabberProtocol>);

JabberProtocol::JabberProtocol(QObject *parent, QString name, QStringList) : KopeteProtocol(parent, name) {
    kdDebug() << "Jabber plugin: Loading ..." << endl;

	if (sProtocol)
		kdDebug() << "[JabberProtocol] Warning: sProtocol already exists! Not redefining." << endl;
	else
		sProtocol = this;
	
    initIcons();
    initActions();

    authContact = new KMessageBox;
	reasonDialog = 0L;

    statusBarIcon = new StatusBarIcon();
    QObject::connect(statusBarIcon, SIGNAL(rightClicked(const QPoint)), this, SLOT(slotIconRightClicked(const QPoint)));
    statusBarIcon->setPixmap(offlineIcon);

    mPrefs = new JabberPreferences("jabber_protocol_32", this);
    connect ( mPrefs, SIGNAL(saved()), this, SLOT(slotSettingsChanged()) );

    KGlobal::config()->setGroup("Jabber");
	if ( (KGlobal::config()->readEntry("UserID", "") == "")
	     || (KGlobal::config()->readEntry("Password", "") == "") )
	{
		KMessageBox::error(
			kopeteapp->mainWindow(),
			i18n("<qt>If you have a Jabber account, please \
			configure it in the Kopete Settings. If you don't, you can register \
			in the settings.</qt>"),
			i18n("No Jabber Configuration found!")
			);
	}

	mIsConnected = false;
	doRegister = false;
	/* Call slotSettingsChanged() to get it all registered. */
	slotSettingsChanged();
   
	if (KGlobal::config()->readBoolEntry("AutoConnect", "0"))
		Connect();
}

JabberProtocol::~JabberProtocol() {
}

void JabberProtocol::init() {
	/* Everybody loves a no-op! */
}

bool JabberProtocol::unload() {
	kdDebug() << "[JabberProtocol] Unload..." << endl;
	if (kopeteapp->statusBar())
	{
		kopeteapp->statusBar()->removeWidget(statusBarIcon);
		delete statusBarIcon;
	}

	emit protocolUnloading();
	return true;
}

void JabberProtocol::Connect() {
	kdDebug() << "[JabberProtocol] Connect()" << endl;
	if ( !isConnected() )
	{
		if (!mStatus)
			mStatus = STATUS_ONLINE;
		
		mProtocol = new Jabber;
		connect(mProtocol, SIGNAL(connected()), this, SLOT(slotConnected()));
		connect(mProtocol, SIGNAL(disconnected()), this, SLOT(slotDisconnected()));
		connect(mProtocol, SIGNAL(contactNew(JabRosterEntry *)), this, SLOT(slotNewContact(JabRosterEntry *)));
		connect(mProtocol, SIGNAL(contactChanged(JabRosterEntry *)), this, SLOT(slotContactUpdated(JabRosterEntry *)));
		connect(mProtocol, SIGNAL(resourceAvailable(const Jid &, const JabResource &)), this, SIGNAL(resourceAvailable(const Jid &, const JabResource &)));
		connect(mProtocol, SIGNAL(resourceUnavailable(const Jid &)), this, SIGNAL(resourceUnavailable(const Jid &)));
		connect(mProtocol, SIGNAL(authRequest(const Jid &)), this, SLOT(slotUserWantsAuth(const Jid &)));
		connect(mProtocol, SIGNAL(messageReceived(const JabMessage &)), this, SLOT(slotNewMessage(const JabMessage &)));
		connect(mProtocol, SIGNAL(error(JabError *)), this, SLOT(slotError(JabError *)));
		
		kdDebug() << "[JabberProtocol] Connecting to Jabber server " << mServer << ":" << mPort << endl;
		kdDebug() << "                 with UserID " << mUsername << endl;
		
		KGlobal::config()->setGroup("Jabber");
		mProtocol->setHost(mServer, mPort);
		mProtocol->setAccount(mUsername, mPassword, mResource);
		mProtocol->setSSLEnabled(KGlobal::config()->readBoolEntry("UseSSL", "0"));
		
		if (doRegister)
		{
			kdDebug() << "[JabberProtocol] Registering user" << endl;
			mProtocol->accRegister();
		}
		else
		{
			kdDebug() << "[JabberProtocol] Already registered, performing normal login." << endl;
			mProtocol->login(mStatus, "", 1, true);
		}
		slotConnecting();
    } 
	else if ( isAway() )
	{	/* They're really away, and they want to un-away. */
		slotGoOnline();
	} 
	else
	{	/* Nope, just your regular crack junky. */
		kdDebug() << "[JabberProtocol] Ignoring connect request (already connected)." << endl;
	}
}

const JabberProtocol *JabberProtocol::protocol() {
	return sProtocol;
}

void JabberProtocol::Disconnect() {
	if (isConnected())
	{
		mProtocol->disc();
		delete mProtocol;
		kdDebug() << "[JabberProtocol] Disconnected." << endl;
		mIsConnected = false;
		statusBarIcon->setPixmap(offlineIcon);
		
		QMap<QString, JabberContact*>::Iterator it;
		for (it = contactList.begin(); it != contactList.end(); ++it)
		{
			kdDebug() << "[JabberProtocol] deleting contact " << it.key() << endl;
			delete it.data();
			it.data() = NULL;
		}
	} 
	else
	{			/* Again, what's with the crack? Sheez. */
		kdDebug() << "Jabber plugin: Ignoring disconnect request (not connected)." << endl;
	}
}

void JabberProtocol::slotConnect() {
	Connect();
}

void JabberProtocol::slotDisconnect() {
	Disconnect();
}

void JabberProtocol::slotConnecting() {
	statusBarIcon->setMovie(connectingIcon);
}

void JabberProtocol::slotConnected() {
	mIsConnected = true;
	kdDebug() << "Jabber plugin: Connected to Jabber server." << endl;
	
	if (mStatus == STATUS_AWAY || mStatus == STATUS_XA)
	{
		statusBarIcon->setPixmap(awayIcon);
	}
	else if (mStatus == STATUS_DND)
	{
		statusBarIcon->setPixmap(naIcon);
	}
	else
	{
		statusBarIcon->setPixmap(onlineIcon);
	}
	myContact = new JabberContact(QString("%1@%2").arg(mUsername, 1).arg(mServer, 2), mUsername, i18n("Unknown"), this);
}

void JabberProtocol::slotDisconnected() {
	mIsConnected = false;
	kdDebug() << "Jabber plugin: Disconnected from Jabber server." << endl;
	statusBarIcon->setPixmap(offlineIcon);
	emit nukeContacts(false);
}

void JabberProtocol::slotError(JabError *error) { /* "Bugger." */
	switch(error->type)
	{
		case JABERR_CONNECT:
			KMessageBox::error(kopeteapp->mainWindow(), i18n("Sorry, but there was an error connecting to the Jabber server (%1).").arg(error->msg, 1), i18n("Error connecting to Jabber server"));
			break;
		case JABERR_AUTH:   /* FIXME FIXME FIXME FIXME!!! */
		case JABERR_CREATE: /* Isn't red-on-orange just so PRETTY? */
		default: /* o/~ cause i'm that fool who broke the key/i'm unlockable so don't check me/
					    i got weight on my shoulders and things on my mind/ the sky is falling, and i'm falling behind */
			KMessageBox::error(kopeteapp->mainWindow(), i18n("Sorry, but you were disconnected for an unspecified reason (%1).").arg(error->type, 1), i18n("Disconnected from Jabber server")); /* (But were they really disconnected, or are they just on speed? */
	}
}

bool JabberProtocol::isConnected() const {
    return mIsConnected;
}

void JabberProtocol::setAway(void) {
    setPresence(STATUS_AWAY, "Automatically set away due to being idle.");
}

void JabberProtocol::setAvailable(void) {
    slotGoOnline();
}

bool JabberProtocol::isAway(void) const
{
	if (isConnected())
		return (mProtocol->status() == STATUS_AWAY || mProtocol->status() == STATUS_XA || mProtocol->status() == STATUS_DND);
	else 
		return false; /* Well, technically we're not away, but then again, we're not online either, so this
					   * statistic isn't *really* that useful. */
}

QString JabberProtocol::protocolIcon() const {
    return "jabber_protocol_32";
}

AddContactPage *JabberProtocol::createAddContactWidget(QWidget *parent) {
    return new JabberAddContactPage(this, parent);
}

void JabberProtocol::initIcons() {
	KIconLoader *loader = KGlobal::iconLoader();
	KStandardDirs dir;

	onlineIcon = QPixmap(loader->loadIcon("jabber_online", KIcon::User));
	offlineIcon = QPixmap(loader->loadIcon("jabber_offline", KIcon::User));
	awayIcon = QPixmap(loader->loadIcon("jabber_away", KIcon::User));
	naIcon = QPixmap(loader->loadIcon("jabber_na", KIcon::User));
	connectingIcon = QMovie(dir.findResource("data", "kopete/pics/jabber_connecting.mng"));
}

void JabberProtocol::initActions() {
	actionGoOnline =
	new KAction(i18n("Online"), "jabber_online", 0, this,
			SLOT(slotConnect()), this, "actionJabberConnect");
	actionGoAway =
	new KAction(i18n("Away"), "jabber_away", 0, this,
			SLOT(slotSetAway()), this, "actionJabberway");
	actionGoXA =
	new KAction(i18n("Extended Away"), "jabber_away", 0, this,
			SLOT(slotSetXA()), this, "actionJabberXA");
	actionGoDND =
	new KAction(i18n("Do Not Disturb"), "jabber_na", 0, this,
			SLOT(slotSetDND()), this, "actionJabberDND");
	actionGoOffline =
	new KAction(i18n("Offline"), "jabber_offline", 0, this,
			SLOT(slotDisconnect()), this, "actionJabberDisconnect");
	actionStatusMenu = new KActionMenu("Jabber", this);
	actionStatusMenu->insert(actionGoOnline);
	actionStatusMenu->insert(actionGoAway);
	actionStatusMenu->insert(actionGoXA);
	actionStatusMenu->insert(actionGoDND);
	actionStatusMenu->insert(actionGoOffline);
	actionStatusMenu->plug(kopeteapp->systemTray()->contextMenu(), 1);
}

void JabberProtocol::slotGoOnline()
{
    kdDebug() << "[JabberProtocol] Going online!" << endl;

	if (!isConnected())
	{
		mStatus = STATUS_ONLINE;
		Connect();
	}
	else
	{
		setPresence(STATUS_ONLINE, "");
	}
	statusBarIcon->setPixmap(onlineIcon);
}

void JabberProtocol::slotGoOffline() {
    kdDebug() << "Jabber plugin: Going offline." << endl;
    Disconnect();
    statusBarIcon->setPixmap(offlineIcon);
}

void JabberProtocol::slotSetAway()
{
	kdDebug() << "Jabber plugin: Setting away mode." << endl;
	if (reasonDialog != 0L)
	{
		delete reasonDialog;
	}
	reasonDialog = new dlgJabberStatus(this, STATUS_AWAY, kopeteapp->mainWindow());
}

void JabberProtocol::slotSetXA() {
    kdDebug() << "Jabber plugin: Setting extended away mode." << endl;
	if (reasonDialog != 0L) {
		delete reasonDialog;
	}
	reasonDialog = new dlgJabberStatus(this, STATUS_XA, kopeteapp->mainWindow());
}

void JabberProtocol::slotSetDND() {
    kdDebug() << "Jabber plugin: Setting do not disturb mode." << endl;
	if (reasonDialog != 0L) {
		delete reasonDialog;
	}
	reasonDialog = new dlgJabberStatus(this, STATUS_DND, kopeteapp->mainWindow());
}

void JabberProtocol::setPresence(int status, QString reason, int priority)
{
	if (mIsConnected)
	{ 
		mProtocol->setPresence(status, reason, priority);
		if (status == STATUS_AWAY || status == STATUS_XA) { statusBarIcon->setPixmap(awayIcon); }
		else if (status == STATUS_DND) { statusBarIcon->setPixmap(naIcon); }
		else if (status == STATUS_ONLINE) { statusBarIcon->setPixmap(onlineIcon); }
		else
		{ /* What You Say!! */
			kdDebug() << "Jabber plugin: setPresence as confused as a virgin in a brothel (status == " << status << ")!!" << endl;
			mProtocol->setPresence(STATUS_ONLINE, "", priority);
			kdDebug() << "Jabber plugin: Took corrective measures - went online. Someone needs a good LARTing." << endl;
			statusBarIcon->setPixmap(onlineIcon);
		}
	}
}

void JabberProtocol::slotIconRightClicked(const QPoint)
{
	QString handle = mUsername + "@" + mServer;
	popup = new KPopupMenu(statusBarIcon);
	popup->insertTitle(handle);
	actionGoOnline->plug(popup);
	actionGoOffline->plug(popup);
	actionGoAway->plug(popup);
	actionGoXA->plug(popup);
	actionGoDND->plug(popup);
	popup->popup(QCursor::pos());
}

void JabberProtocol::removeUser(QString userID) {
	if (mIsConnected) { 
		kdDebug() << "Jabber plugin: Protocol removing user " << userID << endl;
	    mProtocol->unsubscribed(userID);
		mProtocol->unsubscribe(userID);
		kdDebug() << "Jabber plugin: I'M A SOLDAR" << endl;
		contactList[userID] = 0L;
		kdDebug() << "Jabber plugin: I'M A LOITNUNT" << endl;
	}
}

void JabberProtocol::renameContact(QString userID, QString name, QString group) {
    if (mIsConnected) {
		kdDebug() << "Jabber plugin: Protocol renaming user " << userID << " to " << name << endl;
	    mProtocol->setRoster(userID, name, group);
	}
}

void JabberProtocol::moveUser(QString userID, QString group, QString name, JabberContact *contact) {
	if (mIsConnected) {
		QString localGroup;
		if (group == QString("")) { 
			kdDebug() << "Jabber plugin: Protocol moving user " << userID << " out of a group." << endl;
			localGroup = i18n("Unknown");
		}
		else { 
			kdDebug() << "[JabberProtocol] Protocol moving user " << userID << " to " << group << endl;
			localGroup = group;
		}
	    mProtocol->setRoster(userID, name, group);
		kopeteapp->contactList()->moveContact(contact, localGroup);
	}
}

void JabberProtocol::addContact(QString userID) {
    if (mIsConnected) {
		kdDebug() << "[JabberProtocol] Protocol adding user " << userID << endl;
	    mProtocol->subscribe(userID);
	}
}

void JabberProtocol::slotUserWantsAuth(const Jid &jid) {
	QString userID = QString("%1@%2").arg(jid.user(), 1).arg(jid.host(), 2);
	kdDebug() << "[JabberProtocol] " << userID << " wants auth!" << endl;
	if (authContact->
		questionYesNo(kopeteapp->mainWindow(), i18n("The Jabber user ") + userID +
	    i18n(" wants to add you to your contact list. Do you want to \
			   authorize them?"),
		i18n("Authorize Jabber user?")) == 3) {
			mProtocol->subscribed(userID);
    }
}

void JabberProtocol::slotContactUpdated(JabRosterEntry *contact) {
    emit contactUpdated(contact->jid.latin1(), contact->nick.latin1(), contact->localStatus(), contact->unavailableStatusString.latin1());
}

void JabberProtocol::slotNewContact(JabRosterEntry *contact) {
	/* !!KLUDGE!!.
	 * o/~ don't mean to brag, don't mean to boast/but I'm intercontinental when I eat French toast */
	if (contactList[contact->jid])
	{
		kdDebug() << "[JabberProtocol] Entry already exists for " << contact->jid << endl; return;
	}
	QString group = *(contact->groups.begin());

	KopeteContactList *l = KopeteContactList::contactList();
	KopeteMetaContact *m = l->findContact(contact->jid);
	KopeteContact *c = m->findContact(contact->jid);

	if (c) {
		/* Existing contact, update data. */
		c->initContact(contact->nick, group ? group : QString(""));
	}
	else {
		kdDebug() << "[JabberProtocol] Adding contact " << contact->jid << " ..." << endl;
		JabberContact *jabContact = new JabberContact(contact->jid, contact->nick, group ? group : QString(""), this);
		contactList[contact->jid] = jabContact;
		c = jabContact;
		kdDebug() << "[JabberProtocol] Contact has been added to contactList[]" << endl;
		kdDebug() << "[JabberProtocol] New Contact's userID is " << (contactList[contact->jid])->userID() << endl;
		m->addContact(c, group ? QStringList(group) : QStringList("Unknown"));
	}

	slotContactUpdated(contact); /* More kludges! Ugh. */
}

KopeteContact *JabberProtocol::createContact(KopeteMetaContact *parent, QString &data) {
	/* Assumption: data is just the JID; this could change at some stage. */
	addContact(data);
	if (contactList[data]) { /* EEK! Someone's already added it! */
		return;
	}
	contactList[data] = new JabberContact(data, "", QString(""), this);
	return contactList[data]; /* o/~ superstylin */
}

void JabberProtocol::slotSettingsChanged() {
	mUsername = KGlobal::config()->readEntry("UserID", "");
	mPassword = KGlobal::config()->readEntry("Password", "");
	mResource = KGlobal::config()->readEntry("Resource", "Kopete");
	mServer = KGlobal::config()->readEntry("Server", "jabber.org");
	mPort = KGlobal::config()->readNumEntry("Port", 5222);
}

void JabberProtocol::slotSendMsg(JabMessage message){
	if (mIsConnected)
		mProtocol->sendMessage(message);
}

void JabberProtocol::slotNewMessage(const JabMessage &message) {
	QString jid = QString("%1@%2").arg(message.from.user(), 1).arg(message.from.host(), 2);
	if (message.from.user() == QString("")) {
		kdDebug() << "[JabberProtocol] New server message for us!" << endl;
		KMessageBox::information(kopeteapp->mainWindow(), message.body, i18n("Jabber: Server message"));
	}
	else
	{
		kdDebug() << "[JabberProtocol] New message from '" << jid << "'" << endl;
		JabberContact *ourContact = contactList[jid];
		if (!ourContact)
		{
			kdDebug() << "[JabberProtocol] Crap! No contact, gack." << endl;
			return;
		}
		ourContact->slotNewMessage(message);
	}
}

void JabberProtocol::registerUser() {
	mPrefs->save();
	doRegister = true;
	Connect();
	doRegister = false;
}

#include "jabberprotocol.moc"

// vim: set ts=4 sts=4 sw=4 noet:
