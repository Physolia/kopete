/*
  aimcontact.cpp  -  Oscar Protocol Plugin

  Copyright (c) 2003 by Will Stephenson
  Copyright (c) 2006 by Roman Jarosz <kedgedev@centrum.cz>
  Kopete    (c) 2002-2006 by the Kopete developers  <kopete-devel@kde.org>

  *************************************************************************
  *                                                                       *
  * This program is free software; you can redistribute it and/or modify  *
  * it under the terms of the GNU General Public License as published by  *
  * the Free Software Foundation; either version 2 of the License, or     *
  * (at your option) any later version.                                   *
  *                                                                       *
  *************************************************************************
*/

#include "aimcontact.h"

#include <klocale.h>
#include <ktoggleaction.h>
#include <kicon.h>

//liboscar
#include "oscarutils.h"
#include "contactmanager.h"

#include "icqprotocol.h"
#include "icqaccount.h"

AIMContact::AIMContact( Kopete::Account* account, const QString& name, Kopete::MetaContact* parent,
                        const QString& icon, const OContact& ssiItem )
: AIMContactBase(account, name, parent, icon, ssiItem )
{
	mProtocol=static_cast<ICQProtocol *>(protocol());
	setOnlineStatus( ICQ::Presence( ICQ::Presence::Offline, ICQ::Presence::Visible ).toOnlineStatus() );

	QObject::connect( mAccount->engine(), SIGNAL( receivedUserInfo( const QString&, const UserDetails& ) ),
	                  this, SLOT( userInfoUpdated( const QString&, const UserDetails& ) ) );
	QObject::connect( mAccount->engine(), SIGNAL( userIsOffline( const QString& ) ),
	                  this, SLOT( userOffline( const QString& ) ) );
}

AIMContact::~AIMContact()
{
}

bool AIMContact::isReachable()
{
	return account()->isConnected();
}

QList<KAction*> *AIMContact::customContextMenuActions()
{
	QList<KAction*> *actionCollection = new QList<KAction*>();

	m_actionIgnore = new KToggleAction(i18n("&Ignore"), this );
        //, "actionIgnore");
	QObject::connect( m_actionIgnore, SIGNAL(triggered(bool)), this, SLOT(slotIgnore()) );

	m_actionVisibleTo = new KToggleAction(i18n("Always &Visible To"), this );
        //, "actionVisibleTo");
	QObject::connect( m_actionVisibleTo, SIGNAL(triggered(bool)), this, SLOT(slotVisibleTo()) );

	m_actionInvisibleTo = new KToggleAction(i18n("Always &Invisible To"), this );
        //, "actionInvisibleTo");
	QObject::connect( m_actionInvisibleTo, SIGNAL(triggered(bool)), this, SLOT(slotInvisibleTo()) );

	m_selectEncoding = new KAction( i18n( "Select Encoding..." ), this );
        //, "changeEncoding" );
	m_selectEncoding->setIcon( KIcon( "charset" ) );
	QObject::connect( m_selectEncoding, SIGNAL(triggered(bool)), this, SLOT(changeContactEncoding()) );

	bool on = account()->isConnected();
	m_actionIgnore->setEnabled(on);
	m_actionVisibleTo->setEnabled(on);
	m_actionInvisibleTo->setEnabled(on);

	ContactManager* ssi = account()->engine()->ssiManager();
	m_actionIgnore->setChecked( ssi->findItem( m_ssiItem.name(), ROSTER_IGNORE ));
	m_actionVisibleTo->setChecked( ssi->findItem( m_ssiItem.name(), ROSTER_VISIBLE ));
	m_actionInvisibleTo->setChecked( ssi->findItem( m_ssiItem.name(), ROSTER_INVISIBLE ));

	actionCollection->append( m_selectEncoding );

	actionCollection->append(m_actionIgnore);
	actionCollection->append(m_actionVisibleTo);
	actionCollection->append(m_actionInvisibleTo);

	return actionCollection;
}

void AIMContact::updateSSIItem()
{
	if ( m_ssiItem.type() != 0xFFFF && m_ssiItem.waitingAuth() == false &&
	     onlineStatus() == Kopete::OnlineStatus::Unknown )
	{
		//make sure they're offline
		setOnlineStatus( ICQ::Presence( ICQ::Presence::Offline, ICQ::Presence::Visible ).toOnlineStatus() );
	}
}

void AIMContact::userInfoUpdated( const QString& contact, const UserDetails& details )
{
	if ( Oscar::normalize( contact ) != Oscar::normalize( contactId() ) )
		return;

	kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << contact << endl;

	//if they don't have an SSI alias, make sure we use the capitalization from the
	//server so their contact id looks all pretty.
	QString nickname = property( Kopete::Global::Properties::self()->nickName() ).value().toString();
	if ( nickname.isEmpty() || Oscar::normalize( nickname ) == Oscar::normalize( contact ) )
		setNickName( contact );

	( details.userClass() & CLASS_WIRELESS ) ? m_mobile = true : m_mobile = false;

	if ( ( details.userClass() & CLASS_AWAY ) == STATUS_ONLINE )
	{
		if ( m_mobile )
		{
			kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "Contact: " << contact << " is mobile-online." << endl;
//TODO:		setOnlineStatus( mProtocol->statusWirelessOnline );
			setOnlineStatus( ICQ::Presence( ICQ::Presence::Online, ICQ::Presence::Visible ).toOnlineStatus() );
    	}
		else
		{
			kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "Contact: " << contact << " is online." << endl;
			setOnlineStatus( ICQ::Presence( ICQ::Presence::Online, ICQ::Presence::Visible ).toOnlineStatus() );
		}

		removeProperty( mProtocol->awayMessage );
		m_haveAwayMessage = false;
	}
	else if ( ( details.userClass() & CLASS_AWAY ) ) // STATUS_AWAY
	{
		if ( m_mobile )
		{
			kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "Contact: " << contact << " is mobile-away." << endl;
//TODO:		setOnlineStatus( mProtocol->statusWirelessOnline );
			setOnlineStatus( ICQ::Presence( ICQ::Presence::Away, ICQ::Presence::Visible ).toOnlineStatus() );
		}
		else
		{
			kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "Contact: " << contact << " is away." << endl;
			setOnlineStatus( ICQ::Presence( ICQ::Presence::Away, ICQ::Presence::Visible ).toOnlineStatus() );
		}

		if ( !m_haveAwayMessage ) //prevent cyclic away message requests
		{
			mAccount->engine()->requestAIMAwayMessage( contactId() );
			m_haveAwayMessage = true;
		}
	}
	else
	{
		kDebug(OSCAR_ICQ_DEBUG) << k_funcinfo << "Contact: " << contact << " class " << details.userClass() << " is unhandled... defaulting to away." << endl;
		setOnlineStatus( ICQ::Presence( ICQ::Presence::Away, ICQ::Presence::Visible ).toOnlineStatus() );
		if ( !m_haveAwayMessage ) //prevent cyclic away message requests
		{
			mAccount->engine()->requestAIMAwayMessage( contactId() );
			m_haveAwayMessage = true;
		}
	}

	OscarContact::userInfoUpdated( contact, details );
}

void AIMContact::userOnline( const QString& userId )
{
	if ( Oscar::normalize( userId ) != Oscar::normalize( contactId() ) )
		return;

	kDebug(OSCAR_ICQ_DEBUG) << "Setting " << userId << " online" << endl;
	ICQ::Presence online = mProtocol->statusManager()->presenceOf( ICQ::Presence::Online );
	//mAccount->engine()->requestStatusInfo( contactId() );
}

void AIMContact::userOffline( const QString& userId )
{
	if ( Oscar::normalize( userId ) != Oscar::normalize( contactId() ) )
		return;

	kDebug(OSCAR_ICQ_DEBUG) << "Setting " << userId << " offline" << endl;
	ICQ::Presence offline = mProtocol->statusManager()->presenceOf( ICQ::Presence::Offline );
	setOnlineStatus( mProtocol->statusManager()->onlineStatusOf( offline ) );
}

void AIMContact::slotVisibleTo()
{
	account()->engine()->setVisibleTo( contactId(), m_actionVisibleTo->isChecked() );
}

void AIMContact::slotIgnore()
{
	account()->engine()->setIgnore( contactId(), m_actionIgnore->isChecked() );
}

void AIMContact::slotInvisibleTo()
{
	account()->engine()->setInvisibleTo( contactId(), m_actionInvisibleTo->isChecked() );
}

#include "aimcontact.moc"
//kate: tab-width 4; indent-mode csands;
