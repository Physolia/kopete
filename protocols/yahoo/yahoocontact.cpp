/*
    yahoocontact.cpp - Yahoo Contact

    Copyright (c) 2002 by Duncan Mac-Vicar Prett <duncan@kde.org>

    Portions based on code by Bruno Rodrigues <bruno.rodrigues@litux.org>

    Copyright (c) 2002 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/
#include "kopetemessagemanagerfactory.h"

// Local Includes
#include "yahoodebug.h"
#include "yahoostatus.h"
#include "yahoocontact.h"

// QT Includes
#include <qstring.h>

// KDE Includes
#include <kdebug.h>
#include <kmessagebox.h>

YahooContact::YahooContact(QString userId, QString fullName,
			     YahooProtocol *pluginInstance, KopeteMetaContact *metaContact)
:  KopeteContact( pluginInstance, userId, metaContact)
{
	kdDebug(14180) << "YahooContact::YahooContact("<< userId << ", " << fullName << ")" << endl;

	m_fullName = fullName;
	m_manager = 0L;
	m_status.setStatus(YahooStatus::Offline);

	// Update ContactList
	setDisplayName(m_fullName);
	setOnlineStatus( m_status.translate() );

	// XXX initActions();

	QObject::connect (this , SIGNAL( moved(KopeteMetaContact*,KopeteContact*) ), this, SLOT (slotMovedToMetaContact() ));
	QObject::connect (metaContact , SIGNAL( aboutToSave(KopeteMetaContact*) ), pluginInstance, SLOT (serialize(KopeteMetaContact*) ));
}

YahooContact::~YahooContact()
{
	kdDebug(14180) << "Yahoo::~YahooContact()" << endl;
}

// Return status text
QString YahooContact::statusText() const
{
	kdDebug(14180) << "Yahoo::statusText()";
	return m_status.text();
}

// Return status icon
QString YahooContact::statusIcon() const
{
	return m_status.icon();
}

void YahooContact::setYahooStatus( YahooStatus::Status status_, const QString &msg, int away)
{
	m_status.setStatus(status_);
	setOnlineStatus( m_status.translate() );
}


/* XXX
void YahooContact::slotUpdateStatus(QString status, QString statusText == NULL)
{
	DEBUG(YDMETHOD, "Yahoo::slotUpdateStatus(" << status << ")");

	DEBUG(YDINFO, "Buddy  - updating " << handle << " to "
			<< status << "." << endl;

	if (status != QString("")) {
		mStatus = status;
		kdDebug(14180) << "Yahoo plugin: Updating status." << endl;
	}
	mStatusText = statusText;
	setOnlineStatus( m_status.translate() );
}
*/

bool YahooContact::isOnline() const
{
	kdDebug(14180) << "[YahooContact::isOnline()]" << endl;
	return onlineStatus() != Offline && onlineStatus() != Unknown;
}

bool YahooContact::isReachable()
{
	kdDebug(14180) << "[YahooContact::isReachable()]" << endl;
	return true;
}

QString YahooContact::identityId() const
{
	kdDebug(14180) << "[YahooContact::identityId()]" << endl;
	return QString::null;
}

KopeteMessageManager *YahooContact::manager( bool )
{
	if( !m_manager )
	{
		KopeteContactPtrList m_them;
		m_them.append( this );
		m_manager = KopeteMessageManagerFactory::factory()->create(protocol()->myself(), m_them, protocol() );
		connect( m_manager, SIGNAL( destroyed() ), this, SLOT( slotMessageManagerDestroyed() ) );
		connect( m_manager, SIGNAL( messageSent(KopeteMessage &, KopeteMessageManager *) ), this, SLOT( slotSendMessage(KopeteMessage &, KopeteMessageManager * ) ) );
	}

	return m_manager;
}

void YahooContact::slotSendMessage(KopeteMessage &message, KopeteMessageManager * )
{
	KopeteContactPtrList m_them = manager()->members();
	KopeteContact *target = m_them.first();
	YahooProtocol *p = static_cast<YahooProtocol*>( protocol() );
	p->yahooSession()->sendIm( p->myself()->identityId(), target->identityId(), message.escapedBody() );

	// append message to window
	manager()->appendMessage(message);
	manager()->messageSucceeded();
}

void YahooContact::slotTyping(bool isTyping_ )
{
	KopeteContactPtrList m_them = manager()->members();
	KopeteContact *target = m_them.first();
	YahooProtocol *p = static_cast<YahooProtocol*>( protocol() );
	p->yahooSession()->sendTyping( p->myself()->identityId(), target->identityId(), isTyping_ );
}

void YahooContact::slotMessageManagerDestroyed()
{
	m_manager = 0L;
}

/*
QPixmap YahooContact::scaledStatusIcon(int size)
{

}
*/

int YahooContact::importance() const
{
	kdDebug(14180) << "[YahooContact::importance()]" << endl;
	return 0;
}

KActionCollection *YahooContact::customContextMenuActions()
{
	kdDebug(14180) << "[YahooContact::customContextMenuActions()]" << endl;
	return 0L;
}

void YahooContact::slotViewHistory()
{
	kdDebug(14180) << "[YahooContact::slotViewHistory()]" << endl;
}

void YahooContact::slotDeleteContact()
{
	kdDebug(14180) << "[YahooContact::slotDeleteContact()]" << endl;
}

void YahooContact::slotUserInfo()
{
	kdDebug(14180) << "[YahooContact::slotUserInfo()]" << endl;
}

void YahooContact::slotSendFile()
{
	kdDebug(14180) << "[YahooContact::slotSendFile()]" << endl;
}

#include "yahoocontact.moc"

// vim: set noet ts=4 sts=4 sw=4:

