/*
    linkaddressbookui.cpp

    This code was shamelessly stolen from kopete's add new contact wizard, used in
    Konversation, and then reappropriated by Kopete.

    Copyright (c) 2004 by John Tapsell           <john@geola.co.uk>
    Copyright (c) 2003-2005 by Will Stephenson   <will@stevello.free-online.co.uk>
    Copyright (c) 2002 by Nick Betcher           <nbetcher@kde.org>
    Copyright (c) 2002 by Duncan Mac-Vicar Prett <duncan@kde.org>

    Kopete    (c) 2002-2004 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include <qcheckbox.h>
#include <kapplication.h>
#include <kconfig.h>
#include <klocale.h>
#include <kiconloader.h>

#include <kdeversion.h>
#include <kinputdialog.h>

#include <kpushbutton.h>
#include <kactivelabel.h>
#include <kdebug.h>
#include <klistview.h>
#include <klistviewsearchline.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <kabc/addressbook.h>

#include "kopetemetacontact.h"

#include "linkaddressbookui.h"
#include "linkaddressbookui_base.h"
#include "addresseeitem.h"
#include "kabcpersistence.h"

LinkAddressbookUI::LinkAddressbookUI(Kopete::MetaContact * mc, QWidget *parent, const char *name )
		: KDialogBase( Plain, i18n("Select Address Book Entry"), Ok|Cancel|Help, Ok, parent, name), m_mc( mc )
{
	QFrame* page = plainPage();
	QGridLayout* pageLayout = new QGridLayout(page, 1, 1, 0, 0);
	m_mainWidget = new LinkAddressbookUI_Base(page);
	pageLayout->addWidget(m_mainWidget, 0, 0);

	enableButtonOK(false);
	setHelp("linkaddressbook");
	m_addressBook = Kopete::KABCPersistence::self()->addressBook();

	// Addressee validation connections
	connect( m_mainWidget->addAddresseeButton, SIGNAL( clicked() ), SLOT( slotAddAddresseeClicked() ) );
	connect( m_mainWidget->addresseeListView, SIGNAL( clicked(QListViewItem * ) ),
			SLOT( slotAddresseeListClicked( QListViewItem * ) ) );
	connect( m_mainWidget->addresseeListView, SIGNAL( selectionChanged( QListViewItem * ) ),
			SLOT( slotAddresseeListClicked( QListViewItem * ) ) );
	connect( m_mainWidget->addresseeListView, SIGNAL( spacePressed( QListViewItem * ) ),
			SLOT( slotAddresseeListClicked( QListViewItem * ) ) );
	
	connect( m_addressBook, SIGNAL( addressBookChanged( AddressBook * ) ), this, SLOT( slotLoadAddressees() ) );
	connect( Kopete::KABCPersistence::self()->addressBook(), SIGNAL( addresseesChanged()), this, SLOT(slotLoadAddressees()));
	
	//We should add a clear KAction here.  But we can't really do that with a designer file :\  this sucks

	m_mainWidget->addresseeListView->setColumnText(2, SmallIconSet("email"), i18n("Email") );

	m_mainWidget->kListViewSearchLine->setListView(m_mainWidget->addresseeListView);
	slotLoadAddressees();

	m_mainWidget->addresseeListView->setColumnWidthMode(0, QListView::Manual);
	m_mainWidget->addresseeListView->setColumnWidth(0, 63); //Photo is 60, and it's nice to have a small gap, imho
}


LinkAddressbookUI::~LinkAddressbookUI()
{
}


KABC::Addressee LinkAddressbookUI::addressee()
{
	return m_addressee;
}

/**  Read in contacts from addressbook, and select the contact that is for our nick. */
void LinkAddressbookUI::slotLoadAddressees()
{
  m_mainWidget->addresseeListView->clear();

  QString realname;

  KABC::AddressBook::Iterator it;
	AddresseeItem * current = 0;
  for( it = m_addressBook->begin(); it != m_addressBook->end(); ++it )
	{
		AddresseeItem * addr = new AddresseeItem( m_mainWidget->addresseeListView, (*it));
		if ( m_mc->metaContactId() == it->uid() )
		{
			m_mainWidget->addresseeListView->setSelected( addr, true );
			current = addr;
		}
	}
	if ( current )
	{
		m_mainWidget->addresseeListView->ensureItemVisible( current );
	}
	enableButtonOK( current );
	m_mainWidget->lblHeader->setText(i18n("Choose the person who '%1' is.").arg(m_mc->displayName() ));
}

void LinkAddressbookUI::slotAddAddresseeClicked()
{
	// Pop up add addressee dialog
	QString addresseeName = KInputDialog::getText( i18n( "New Address Book Entry" ),
					i18n( "Name the new entry:" ),
					m_mc->displayName(), 0, this );

	if ( !addresseeName.isEmpty() )
	{
		KABC::Addressee addr;
		addr.setNameFromString( addresseeName );
		m_addressBook->insertAddressee(addr);
		Kopete::KABCPersistence::self()->writeAddressBook( 0 );
		slotLoadAddressees();
		// select the addressee we just added
		QListViewItem * added = m_mainWidget->addresseeListView->findItem( addresseeName, 1 );
		m_mainWidget->addresseeListView->setSelected( added, true );
		m_mainWidget->addresseeListView->ensureItemVisible( added );
	}
}

void LinkAddressbookUI::slotAddresseeListClicked( QListViewItem *addressee )
{
	// enable ok if a valid addressee is selected
	enableButtonOK( addressee ? addressee->isSelected() : false);
}

void LinkAddressbookUI::accept()
{
	AddresseeItem *item = 0L;
	item = static_cast<AddresseeItem *>( m_mainWidget->addresseeListView->selectedItem() );

	if ( item )
		m_addressee = item->addressee();

	
	disconnect( m_addressBook, SIGNAL( addressBookChanged( AddressBook * ) ), this, SLOT( slotLoadAddressees() ) );
	deleteLater();
	QDialog::accept();
}

void LinkAddressbookUI::reject()
{
  disconnect( m_addressBook, SIGNAL( addressBookChanged( AddressBook * ) ), this, SLOT( slotLoadAddressees() ) );
  deleteLater();
	QDialog::reject();
}

#include "linkaddressbookui.moc"

// vim: set noet ts=4 sts=4 sw=4:

