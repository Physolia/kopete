/*
    kopetepasswordedaccount.cpp - Kopete Account with a password

    Copyright (c) 2004      by Richard Smith         <kde@metafoo.co.uk>
    Kopete    (c) 2002-2004 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include "kopetepasswordedaccount.h"
#include "kopetepassword.h"
#include "kopeteprotocol.h"
#include "kopeteonlinestatus.h"

#include <klocale.h>

#include <qpixmap.h>

struct Kopete::PasswordedAccount::Private
{
	Private( const QString &group, uint maxLen ) : password( group, maxLen, "mPassword" ) {}
	Kopete::Password password;
	Kopete::OnlineStatus initialStatus;
};

Kopete::PasswordedAccount::PasswordedAccount( Kopete::Protocol *parent, const QString &acctId, uint maxLen, const char *name )
 : Kopete::Account( parent, acctId, name ), d( new Private( configGroup(), maxLen ) )
{
}

Kopete::PasswordedAccount::~PasswordedAccount()
{
	delete d;
}

Kopete::Password &Kopete::PasswordedAccount::password()
{
	return d->password;
}

void Kopete::PasswordedAccount::connect( )
{
	Kopete::OnlineStatus s(Kopete::OnlineStatus::Online);
	connect( s );
}

void Kopete::PasswordedAccount::connect( const Kopete::OnlineStatus& initialStatus )
{
	d->initialStatus = initialStatus;
	QString cached = password().cachedValue();
	if ( !cached.isNull() )
	{
		connectWithPassword( cached );
		return;
	}

	QString prompt = passwordPrompt();
	Kopete::Password::PasswordSource src = password().isWrong() ? Kopete::Password::FromUser : Kopete::Password::FromConfigOrUser;

	password().request( this, SLOT( connectWithPassword( const QString & ) ), accountIcon( Kopete::Password::preferredImageSize() ), prompt, src );
}

QString Kopete::PasswordedAccount::passwordPrompt()
{
	if ( password().isWrong() )
		return i18n( "<b>The password was wrong;</b> please re-enter your password for %1 account <b>%2</b>" ).arg( protocol()->displayName(), accountId() );
	else
		return i18n( "Please enter your password for %1 account <b>%2</b>" ).arg( protocol()->displayName(), accountId() );
}

Kopete::OnlineStatus Kopete::PasswordedAccount::initialStatus()
{
	return d->initialStatus;
}

#include "kopetepasswordedaccount.moc"

// vim: set noet ts=4 sts=4 sw=4:
