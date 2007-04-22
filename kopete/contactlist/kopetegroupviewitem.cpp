/*
    kopeteonlinestatus.cpp - Kopete Online Status

    Copyright (c) 2002-2004 by Olivier Goffart       <ogoffart@kde.org>
    Copyright (c) 2003      by Martijn Klingens      <klingens@kde.org>

    Kopete    (c) 2002-2004 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "kopetegroupviewitem.h"

#include <QtCore/QString>

#include <KIcon>

#include "kopetegroup.h"


KopeteGroupViewItem::KopeteGroupViewItem( Kopete::Group *group )
 : QObject(0), QStandardItem()
{
	m_group = group;

}

KopeteGroupViewItem::~KopeteGroupViewItem()
{
	m_group = 0;
}

void KopeteGroupViewItem::setGroup( Kopete::Group* group )
{
	m_group = group;
	setText( group->displayName() );
	setIcon( KIcon("folder") );
}

Kopete::Group* KopeteGroupViewItem::group() const
{
	return m_group;
}

void KopeteGroupViewItem::groupNameChanged( Kopete::Group* group, const QString& )
{
	if ( group == m_group )
		setText( m_group->displayName() );
}


#include "kopetegroupviewitem.moc"
// vim: set noet ts=4 sts=4 sw=4:


