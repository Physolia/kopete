// -*- Mode: c++-mode; c-basic-offset: 2; indent-tabs-mode: t; tab-width: 2; -*-
//
// Copyright (C) 2003 Grzegorz Jaskiewicz 	<gj at pointblue.com.pl>
// Copyright (C) 2002-2003 Zack Rusin 	<zack@kde.org>
//
// gaduprotocol.cpp
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.

#include <kdebug.h>
#include <kgenericfactory.h>
#include <kconfig.h>

#include <libgadu.h>

#include "gaduaccount.h"
#include "gaducontact.h"
#include "gaduprotocol.h"

#include "gadueditaccount.h"
#include "gaduaddcontactpage.h"

#include "kopeteaccountmanager.h"
#include "kopeteaccount.h"
#include "kopetemetacontact.h"
#include "kopeteglobal.h"

typedef KGenericFactory<GaduProtocol> GaduProtocolFactory;

K_EXPORT_COMPONENT_FACTORY( kopete_gadu, KGenericFactory<GaduProtocol>( "kopete_gadu" )  )

GaduProtocol* GaduProtocol::protocolStatic_ = 0L;

GaduProtocol::GaduProtocol( QObject* parent, const char* name, const QStringList& )
:Kopete::Protocol( GaduProtocolFactory::instance(), parent, name ),
			propFirstName(Kopete::Global::Properties::self()->firstName()),
			propLastName(Kopete::Global::Properties::self()->lastName()),
			propEmail(Kopete::Global::Properties::self()->emailAddress()),
			propAwayMessage(Kopete::Global::Properties::self()->awayMessage()),
			propPhoneNr(Kopete::Global::Properties::self()->privatePhone()),
			defaultAccount_( 0 ),
			gaduStatusBlocked_( Kopete::OnlineStatus::Away, GG_STATUS_BLOCKED, this, GG_STATUS_BLOCKED,
				"gg_ignored", "", i18n( "Blocked" ) ),
			gaduStatusOffline_( Kopete::OnlineStatus::Offline, GG_STATUS_NOT_AVAIL, this, GG_STATUS_NOT_AVAIL,
				"gg_offline", i18n( "Go O&ffline" ), i18n( "Offline" ) ),
			gaduStatusOfflineDescr_( Kopete::OnlineStatus::Away, GG_STATUS_NOT_AVAIL_DESCR, this, GG_STATUS_NOT_AVAIL_DESCR,
				"gg_offline_d", i18n( "Go A&way" ), i18n( "Offline" ) ),
			gaduStatusBusy_(Kopete::OnlineStatus::Away, GG_STATUS_BUSY, this, GG_STATUS_BUSY,
				"gg_busy", i18n( "Go B&usy" ), i18n( "Busy" ) ),
			gaduStatusBusyDescr_(Kopete::OnlineStatus::Away, GG_STATUS_BUSY_DESCR, this, GG_STATUS_BUSY_DESCR,
				"gg_busy_d", i18n( "Go B&usy" ), i18n( "Busy" ) ),
			gaduStatusInvisible_( Kopete::OnlineStatus::Invisible, GG_STATUS_INVISIBLE, this, GG_STATUS_INVISIBLE,
				"gg_invi", i18n( "Go I&nvisible" ), i18n( "Invisible" ) ),
			gaduStatusInvisibleDescr_(Kopete::OnlineStatus::Invisible, GG_STATUS_INVISIBLE_DESCR, this, GG_STATUS_INVISIBLE_DESCR,
				"gg_invi_d", i18n( "Go I&nvisible" ), i18n( "Invisible" ) ),
			gaduStatusAvail_(Kopete::OnlineStatus::Online, GG_STATUS_AVAIL, this, GG_STATUS_AVAIL,
				"gg_online", i18n( "Go &Online" ), i18n( "Online" ) ),
			gaduStatusAvailDescr_(Kopete::OnlineStatus::Online, GG_STATUS_AVAIL_DESCR, this, GG_STATUS_AVAIL_DESCR,
				"gg_online_d", i18n( "Go &Online" ), i18n( "Online" ) ),
			gaduConnecting_(Kopete::OnlineStatus::Offline, GG_STATUS_CONNECTING, this, GG_STATUS_CONNECTING,
				"gg_con", i18n( "Connect" ), i18n( "Connecting" ) )
{
	if ( protocolStatic_ ) {
		kdDebug(14100)<<"####"<<"GaduProtocol already initialized"<<endl;
	}
	else {
		protocolStatic_ = this;
	}

	addAddressBookField( "messaging/gadu", Kopete::Plugin::MakeIndexField );

	setRichTextCapabilities( Kopete::Protocol::RichFormatting | Kopete::Protocol::RichFgColor );

}

GaduProtocol::~GaduProtocol()
{
	protocolStatic_ = 0L;
}

GaduProtocol*
GaduProtocol::protocol()
{
	return protocolStatic_;
}

AddContactPage*
GaduProtocol::createAddContactWidget( QWidget* parent, Kopete::Account* account )
{
	return new GaduAddContactPage( static_cast<GaduAccount*>( account ), parent );
}

void
GaduProtocol::settingsChanged()
{
}

Kopete::Contact *
GaduProtocol::deserializeContact( Kopete::MetaContact* metaContact,
				const QMap<QString, QString>& serializedData,
				const QMap<QString, QString>&  /* addressBookData */ )
{

	const QString aid	= serializedData[ "accountId" ];
	const QString cid	= serializedData[ "contactId" ];
	const QString dn	= serializedData[ "displayName" ];

	QDict<Kopete::Account> daccounts = Kopete::AccountManager::manager()->accounts( this );

	Kopete::Account* account = daccounts[ aid ];
	if (!account) {
		account = createNewAccount(aid);
	}

	GaduAccount* gaccount = static_cast<GaduAccount *>( account );

	GaduContact* contact = new GaduContact( cid.toUInt(), dn, account, metaContact );

	contact->setParentIdentity( aid );
	gaccount->addNotify( cid.toUInt() );

	contact->setProperty( propEmail, serializedData["email"] );
	contact->setProperty( propFirstName, serializedData["FirstName"] );
	contact->setProperty( propLastName, serializedData["SecondName"] );
	contact->setProperty( propPhoneNr, serializedData["telephone"] );
	contact->setIgnored(serializedData["ignored"] == "true");
	return contact;
}

uint
GaduProtocol::statusToWithDescription( Kopete::OnlineStatus status )
{

	if ( status == gaduStatusOffline_ || status == gaduStatusOfflineDescr_ ) {
		return GG_STATUS_NOT_AVAIL_DESCR;
	}

	if ( status == gaduStatusBusyDescr_ || status == gaduStatusBusy_ ){
		return GG_STATUS_BUSY_DESCR;
	}

	if ( status == gaduStatusInvisibleDescr_ || status == gaduStatusInvisible_){
		return GG_STATUS_INVISIBLE_DESCR;
	}

    return GG_STATUS_AVAIL_DESCR;
}

bool
GaduProtocol::statusWithDesciption( uint status )
{
	switch( status ){
		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_BUSY:
		case GG_STATUS_INVISIBLE:
		case GG_STATUS_AVAIL:
		case GG_STATUS_CONNECTING:
		case GG_STATUS_BLOCKED:
			return false;
		case GG_STATUS_INVISIBLE_DESCR:
		case GG_STATUS_NOT_AVAIL_DESCR:
		case GG_STATUS_BUSY_DESCR:
		case GG_STATUS_AVAIL_DESCR:
			return true;
	}
	return false;
}

Kopete::OnlineStatus
GaduProtocol::convertStatus( uint status ) const
{
	switch( status ){
		case GG_STATUS_NOT_AVAIL:
			return gaduStatusOffline_;
		case GG_STATUS_NOT_AVAIL_DESCR:
			return gaduStatusOfflineDescr_;
		case GG_STATUS_BUSY:
			return gaduStatusBusy_;
		case GG_STATUS_BUSY_DESCR:
			return gaduStatusBusyDescr_;
		case GG_STATUS_INVISIBLE:
			return gaduStatusInvisible_;
		case GG_STATUS_INVISIBLE_DESCR:
			return gaduStatusInvisibleDescr_;
		case GG_STATUS_AVAIL:
			return gaduStatusAvail_;
		case GG_STATUS_AVAIL_DESCR:
			return gaduStatusAvailDescr_;
		case GG_STATUS_CONNECTING:
			return gaduConnecting_;
		case GG_STATUS_BLOCKED:
			return gaduStatusBlocked_;
		default:
			return gaduStatusOffline_;
	}
}

Kopete::Account*
GaduProtocol::createNewAccount( const QString& accountId )
{
	defaultAccount_ = new GaduAccount( this, accountId );
	return defaultAccount_ ;
}

KopeteEditAccountWidget*
GaduProtocol::createEditAccountWidget( Kopete::Account* account, QWidget* parent )
{
	return( new GaduEditAccount( this, account, parent ) );
}

#include "gaduprotocol.moc"
