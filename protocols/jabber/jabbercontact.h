 /*
  * jabbercontact.cpp  -  Regular Kopete Jabber protocol contact
  *
  * Copyright (c) 2002-2004 by Till Gerken <till@tantalo.net>
  *
  * Kopete    (c) by the Kopete developers  <kopete-devel@kde.org>
  *
  * *************************************************************************
  * *                                                                       *
  * * This program is free software; you can redistribute it and/or modify  *
  * * it under the terms of the GNU General Public License as published by  *
  * * the Free Software Foundation; either version 2 of the License, or     *
  * * (at your option) any later version.                                   *
  * *                                                                       *
  * *************************************************************************
  */

#ifndef JABBERCONTACT_H
#define JABBERCONTACT_H

#include "jabberbasecontact.h"
#include "xmpp_vcard.h"

#include "kopetemessagemanager.h" // needed for silly KopeteContactPtrList

class JabberMessageManager;

class JabberContact : public JabberBaseContact
{

Q_OBJECT

public:

	JabberContact (const XMPP::RosterItem &rosterItem,
				   JabberAccount *account, Kopete::MetaContact * mc);

	/**
	 * Create custom context menu items for the contact
	 * FIXME: implement manager version here?
	 */
	QPtrList<KAction> *customContextMenuActions ();

	/**
	 * Start a rename request.
	 */
	void rename ( const QString &newName );

	/**
	 * Deal with an incoming message for this contact.
	 */
	void handleIncomingMessage ( const XMPP::Message &message );

	/**
	 * Create a message manager for this contact.
	 * This variant is a pure single-contact version and
	 * not suitable for groupchat, as it only looks for
	 * managers with ourselves in the contact list.
	 */
	Kopete::MessageManager *manager ( bool canCreate = false );
	
	/**
	 * Reads a vCard object and updates the contact's
	 * properties accordingly.
	 */
	void setPropertiesFromVCard ( const XMPP::VCard &vCard );

public slots:

	/**
	 * Remove this contact from the roster
	 */
	void slotDeleteContact ();

	/**
	 * Sync Groups with server
	 */
	void syncGroups ();

	/**
	 * This is the JabberContact level slot for sending files.
	 *
	 * @param sourceURL The actual KURL of the file you are sending
	 * @param fileName (Optional) An alternate name for the file - what the
	 *                 receiver will see
	 * @param fileSize (Optional) Size of the file being sent. Used when sending
	 *                 a nondeterminate file size (such as over a socket)
	 */
	virtual void sendFile( const KURL &sourceURL = KURL(),
		const QString &fileName = QString::null, uint fileSize = 0L );

	/**
	 * Retrieve a vCard for the contact
	 */
	void slotUserInfo ();

private slots:

	/**
	 * Send type="subscribed" to contact
	 */
	void slotSendAuth ();

	/**
	 * Send type="subscribe" to contact
	 */
	void slotRequestAuth ();

	/**
	 * Send type="unsubscribed" to contact
	 */
	void slotRemoveAuth ();

	/**
	 * Change this contact's status
	 */
	void slotStatusOnline ();
	void slotStatusChatty ();
	void slotStatusAway ();
	void slotStatusXA ();
	void slotStatusDND ();
	void slotStatusInvisible ();

	/**
	* Select a new resource for the contact
	*/
	void slotSelectResource ();

	void slotMessageManagerDeleted ( QObject *sender );
	
	/**
	 * Check if cached vCard is recent.
	 * Triggered as soon as Kopete changes its online state.
	 */
	void slotCheckVCard ();
	
	/**
	 * Triggered from a timer, requests the vCard.
	 * Timer is initiated by slotCheckVCard().
	 */
	void slotGetTimedVCard ();
	
	/**
	 * Passes vCard on to parsing function.
	 */
	void slotGotVCard ();

private:

	/**
	 * Create a message manager for this contact.
	 * This variant is a pure single-contact version and
	 * not suitable for groupchat, as it only looks for
	 * managers with ourselves in the contact list.
	 * Additionally to the version above, this one adds
	 * a resource constraint that has to be matched by
	 * the manager. If a new manager is created, the given
	 * resource is preselected.
	 */
	JabberMessageManager *manager ( const QString &resource, bool canCreate = false );

	/**
	 * Create a message manager for this contact.
	 * This version is suitable for group chat as it
	 * looks for a message manager with a given
	 * list of contacts as members.
	 */
	JabberMessageManager *manager ( KopeteContactPtrList chatMembers, bool canCreate = false );

	/**
	 * Sends subscription messages.
	 */
	void sendSubscription (const QString& subType);

	/**
	 * Sends a presence packet to this contact
	 */
	void sendPresence ( const XMPP::Status status );

	/**
	 * This variable keeps a list of message managers.
	 * It is required to locate message managers by
	 * resource name, if one account is interacting
	 * with several resources of the same contact
	 * at the same time. Note that this does *not*
	 * apply to group chats, so this variable
	 * only contains classes of type JabberMessageManager.
	 * The casts in manager() and slotMessageManagerDeleted()
	 * are thus legal.
	 */
	QPtrList<JabberMessageManager> mManagers;

	/**
	 * Indicates whether the vCard is currently
	 * being updated or not.
	 */
	bool mVCardUpdateInProgress;

};

#endif
