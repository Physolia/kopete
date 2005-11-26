/*
    appearanceconfig.cpp  -  Kopete Look Feel Config

    Copyright (c) 2001-2002 by Duncan Mac-Vicar Prett <duncan@kde.org>
    Copyright (c) 2005      by Michaël Larouche       <michael.larouche@kdemail.net>

    Kopete    (c) 2002-2005 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "appearanceconfig.h"
#include "appearanceconfig_emoticons.h"
#include "appearanceconfig_chatwindow.h"
#include "appearanceconfig_colors.h"
#include "appearanceconfig_contactlist.h"

#include "tooltipeditdialog.h"

#include <qcheckbox.h>
#include <qdir.h>
#include <qlayout.h>
#include <qhbuttongroup.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlabel.h>

#include <kdeversion.h>
#include <kinputdialog.h>

#include <kapplication.h>
#include <kcolorcombo.h>
#include <kcolorbutton.h>
#include <kconfig.h> // for KNewStuff emoticon fetching
#include <kdebug.h>
#include <kfontrequester.h>
#include <kgenericfactory.h>
#include <khtmlview.h>
#include <khtml_part.h>
#include <kio/netaccess.h>
#include <klineedit.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <kstandarddirs.h>
#include <ktextedit.h>
#include <kurl.h> // KNewStuff
#include <kurlrequesterdlg.h>
#include <krun.h>
#include <ktar.h> // for extracting tarballed chatwindow styles fetched with KNewStuff
#include <kdirwatch.h>

#include <knewstuff/downloaddialog.h> // knewstuff emoticon and chatwindow fetching
#include <knewstuff/engine.h>         // "
#include <knewstuff/entry.h>          // "
#include <knewstuff/knewstuff.h>      // "
#include <knewstuff/provider.h>       // "
#include <kfilterdev.h>               // knewstuff gzipped file support

// For Kopete Chat Window Style configuration and preview.
#include <kopetechatwindowstylemanager.h>
#include <kopetechatwindowstyle.h>
#include <chatmessagepart.h>

// Things we fake to get the message preview to work
#include <kopeteprotocol.h>
#include <kopetemetacontact.h>
#include <kopeteaccount.h>
#include <kopetecontact.h>
#include <kopetemessage.h>
#include <kopetechatsession.h>
#include <kopetechatsessionmanager.h>

#include "kopeteprefs.h"
#include "kopeteemoticons.h"
#include "kopeteglobal.h"

#include <qtabwidget.h>

typedef KGenericFactory<AppearanceConfig, QWidget> KopeteAppearanceConfigFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_kopete_appearanceconfig, KopeteAppearanceConfigFactory( "kcm_kopete_appearanceconfig" ) )

class FakeProtocol;
class FakeAccount;
class FakeContact;

class AppearanceConfig::Private
{
public:
	Private()
	 : mAppearanceTabCtl(0L), preview(0L), mPrfsEmoticons(0L),mPrfsChatWindow(0L),
	   mPrfsColors(0L), mPrfsContactList(0L), currentStyle(0L), loading(false),
	   styleChanged(false)
	{}

	QTabWidget *mAppearanceTabCtl;
	
	ChatMessagePart *preview;
	AppearanceConfig_Emoticons *mPrfsEmoticons;
	AppearanceConfig_ChatWindow *mPrfsChatWindow;
	AppearanceConfig_Colors *mPrfsColors;
	AppearanceConfig_ContactList *mPrfsContactList;
	
	QMap<QListBoxItem*,ChatWindowStyle*> styleItemMap;
	ChatWindowStyle::StyleVariants currentVariantMap;
	ChatWindowStyle *currentStyle;
	bool loading;
	bool styleChanged;

	// For style preview
	FakeProtocol *previewProtocol;
	FakeAccount *previewAccount;
	Kopete::MetaContact *myselfMetaContact;
	Kopete::MetaContact *jackMetaContact;
	FakeContact *myself;
	FakeContact *jack;
	Kopete::ChatSession *previewChatSession;
};

// TODO: Rewrite KopeteStyleNewStuff, support new theme format and remove bugs.
//       so Get Hot New Stuff will be actived by default.
class KopeteStyleNewStuff : public KNewStuff
{
	public:
	KopeteStyleNewStuff(const QString &type, AppearanceConfig * ac, QWidget *parentWidget=0) 
         : KNewStuff( type, parentWidget ), mAppearanceConfig( ac ), m_integrity( false )
	{ }

	bool createUploadFile(const QString&)
	{ return false; }

	bool install( const QString & fileName)
	{
		QString origFileName = mFilenameMap[ fileName ];
		if ( origFileName.endsWith( ".xsl" ) )
		{
			// copy to apps/kopete/styles
			kdDebug(14000) << k_funcinfo << " installing simple style file: " << origFileName << endl;
			/*
			if( !m_integrity )
			{
				KMessageBox::queuedMessageBox( parentWidget(), KMessageBox::Error,
					i18n( "Package could not be verified. Please contact to author of the package. [Error: 7]" ),
					i18n( "Package Verification Failed" ) );
				return false;
			}
			*/
// 			QString styleSheet = mAppearanceConfig->fileContents(fileName);
// 			if ( Kopete::XSLT( styleSheet ).isValid() )
// 				mAppearanceConfig->addStyle( origFileName.section( '.', 0, 0 ), styleSheet );
// 			QFile::remove( fileName );
// 			mAppearanceConfig->slotLoadStyles();
			return true;
		}
		else if ( origFileName.endsWith( ".tar.gz" ) )
		{
			/* If KNewStuff is forced to be enabled from config file, then no verification is necessary.
			int r;
			if( ( r = verify( fileName ) ) != 0 )
			{
				KMessageBox::queuedMessageBox( parentWidget(), KMessageBox::Error,
					i18n( "Package could not be verified. Please contact to author of the package. [Error: %1]" ).arg( r ),
					i18n( "Package Verification Failed" ) );
				return false;
			}
			*/
			// install a tar.gz
			kdDebug(14000) << k_funcinfo << " extracting gzipped tarball: " << origFileName << endl;
			QString uncompress = "application/x-gzip";
			KTar tar(fileName, uncompress);
			tar.open(IO_ReadOnly);
			const KArchiveDirectory *dir = tar.directory();
			dir->copyTo( locateLocal( "appdata", QString::fromLatin1( "styles" ) ) );
			tar.close();
			QFile::remove(fileName);
			mAppearanceConfig->slotLoadStyles();
			return true;
		}
		else if ( origFileName.endsWith( ".xsl.gz" ) )
		{
			kdDebug(14000) << k_funcinfo << " installing gzipped single style file: " << origFileName << endl;
			/*
			if( !m_integrity )
			{
				KMessageBox::queuedMessageBox( parentWidget(), KMessageBox::Error,
					i18n( "Package could not be verified. Please contact to author of the package. [Error: 7]" ),
					i18n( "Package Verification Failed" ) );
				return false;
			}
			*/
			QIODevice * iod = KFilterDev::deviceForFile( fileName, "application/x-gzip" );
			iod->open( IO_ReadOnly );
			QTextStream stream( iod );
			QString styleSheet = stream.read();
			iod->close();
// 			if ( Kopete::XSLT( styleSheet ).isValid() )
// 				mAppearanceConfig->addStyle( origFileName.section( '.', 0, 0 ), styleSheet );
			QFile::remove( fileName );
			mAppearanceConfig->slotLoadStyles();
			return true;

		}
		else
		{
			/* Commented out due to string freeze.
			KMessageBox::queuedMessageBox( parentWidget(), KMessageBox::Error,
				i18n( "Only allowed package extensions are .xsl, .tar.gz and .xsl.gz" ),
				i18n( "Extension not supported" ) );
			*/
			return false;
		}
	}

	/**
	 * A package named Foo must be packaged as Foo.tar.gz use alphanumeric package
	 * names (i.e. do not use a . in the file name ).
	 * content should be like as follows:
	 * /Foo.xsl
	 * /data/Foo/file1.png
	 * /data/Foo/file2.png
	 * /data/Foo/bar/zoo/boo.png ...
	 *
	 * @param file file name to be verified
	 * @return 0 on success<br>
	 *         1 if root directory contains garbage<br>
	 *         2 data directory does not exists<br>
	 *         3 data directory contains garbage files/dirs<br>
	 *         4 the directory under data/ is not the same name as package<br>
	 *         5 Style file does not exist.<br>
	 *         6 data directory does not contain any files<br>
	 *         7 file does not even exists!
	 *         8 package name must be same with basename of the file
	 */
	int verify( const QString& file )
	{
		QFileInfo i( file );
		if( !i.exists() )
		{
			kdDebug( 14000 ) << k_funcinfo << "ERROR: Could not open file [" << file << "] This should never have happened." << endl;
			return 7; // actually it's pointless to return this, since this is a sign of internal error.
		}

		if( !m_integrity )
		{
			kdDebug( 14000 ) << k_funcinfo << "ERROR: Package name is not the same with basename of the filename." << endl;
			return 8;
		}

		QString base = i.baseName();

		KTar tar( file, "application/x-gzip" );
		tar.open( IO_ReadOnly );
		const KArchiveDirectory *dir = tar.directory();
		QStringList list = dir->entries();

		if( list.count() != 2 )
		{
			kdDebug( 14000 ) << k_funcinfo << "ERROR: Garbage file or directory in root directory of the package" << endl;
			return 1;
		}

		const KArchiveEntry *data = dir->entry( "data" );
		if( !data || !data->isDirectory()  )
		{
			kdDebug( 14000 ) << k_funcinfo << "ERROR: data directory does not exist" << endl;
			return 2;
		}
		else
		{
			list = ((KArchiveDirectory*)data)->entries();
			if( list.count() == 0 )
			{
				kdDebug( 14000 ) << k_funcinfo << "ERROR: There is no file in the data directory. So why use a tarball?" << endl;
				return 6;
			}
			else if( list.count() != 1 )
			{
				kdDebug( 14000 ) << k_funcinfo << "ERROR: data directory contains garbage entries" << endl;
				return 3;
			}
			data = ((KArchiveDirectory*)data)->entry( base );
			if( !data || !data->isDirectory() )
			{
				kdDebug( 14000 ) << k_funcinfo << "ERROR: directory under data dir should have the same name as package" << endl;
				return 4;
			}
		}

		const KArchiveEntry *xsl = dir->entry( base + ".xsl" );
		if( !xsl || !xsl->isFile() )
		{
			kdDebug( 14000 ) << k_funcinfo << "ERROR: Style file does not exist." << endl;
			return 5;
		}

		return 0;

	}

	QString downloadDestination( KNS::Entry * e )
	{
		QString filename = e->payload().fileName();
		QFileInfo i( filename );
		if( e->name() != i.baseName() )
		{
			kdDebug( 14000 ) << k_funcinfo << "ERROR: Package name is not the basename of the file." << endl;
			m_integrity = false;
		}
		else
		{
			m_integrity = true;
		}
		QString tempDestination = KNewStuff::downloadDestination( e );
		mFilenameMap.insert( tempDestination, filename );
		return tempDestination;
	}

	QMap<QString, QString > mFilenameMap;
	AppearanceConfig * mAppearanceConfig;
private:
	bool m_integrity;
};

// TODO: Someday, this configuration dialog must(not should) use KConfigXT
AppearanceConfig::AppearanceConfig(QWidget *parent, const char* /*name*/, const QStringList &args )
: KCModule( KopeteAppearanceConfigFactory::instance(), parent, args )
{
	d = new Private;

	(new QVBoxLayout(this))->setAutoAdd(true);
	d->mAppearanceTabCtl = new QTabWidget(this, "mAppearanceTabCtl");

	KConfig *config = KGlobal::config();
	config->setGroup( "ChatWindowSettings" );

	// "Emoticons" TAB ==========================================================
	d->mPrfsEmoticons = new AppearanceConfig_Emoticons(d->mAppearanceTabCtl);
	connect(d->mPrfsEmoticons->chkUseEmoticons, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsEmoticons->chkRequireSpaces, SIGNAL(toggled(bool)),
			this, SLOT(emitChanged()));
	connect(d->mPrfsEmoticons->icon_theme_list, SIGNAL(selectionChanged()),
		this, SLOT(slotSelectedEmoticonsThemeChanged()));
	connect(d->mPrfsEmoticons->btnInstallTheme, SIGNAL(clicked()),
		this, SLOT(installNewTheme()));

	// Since KNewStuff is incomplete and buggy we'll disable it by default.
	d->mPrfsEmoticons->btnGetThemes->setEnabled( config->readBoolEntry( "ForceNewStuff", false ) );
	connect(d->mPrfsEmoticons->btnGetThemes, SIGNAL(clicked()),
		this, SLOT(slotGetThemes()));
	connect(d->mPrfsEmoticons->btnRemoveTheme, SIGNAL(clicked()),
		this, SLOT(removeSelectedTheme()));

	d->mAppearanceTabCtl->addTab(d->mPrfsEmoticons, i18n("&Emoticons"));

	// "Chat Window" TAB ========================================================
	d->mPrfsChatWindow = new AppearanceConfig_ChatWindow(d->mAppearanceTabCtl);

	// Disable current non-working (and also obsolete) buttons
	// TODO: Remove these following lines when everything will be back to normal.
	d->mPrfsChatWindow->deleteButton->setEnabled(false);
	d->mPrfsChatWindow->importButton->setEnabled(false);
	d->mPrfsChatWindow->copyButton->setEnabled(false);

	connect(d->mPrfsChatWindow->mTransparencyEnabled, SIGNAL(toggled(bool)),
		this, SLOT(slotTransparencyChanged(bool)));
	connect(d->mPrfsChatWindow->styleList, SIGNAL(selectionChanged(QListBoxItem *)),
		this, SLOT(slotStyleSelected()));
	connect(d->mPrfsChatWindow->variantList, SIGNAL(activated(const QString&)),
		this, SLOT(slotVariantSelected(const QString &)));
	connect(d->mPrfsChatWindow->deleteButton, SIGNAL(clicked()),
		this, SLOT(slotDeleteStyle()));
	connect(d->mPrfsChatWindow->importButton, SIGNAL(clicked()),
		this, SLOT(slotImportStyle()));
	connect(d->mPrfsChatWindow->copyButton, SIGNAL(clicked()),
		this, SLOT(slotCopyStyle()));
	connect(d->mPrfsChatWindow->btnGetStyles, SIGNAL(clicked()),
		this, SLOT(slotGetStyles()));
	// Show the available styles when the Manager has finish to load the styles.
	connect(ChatWindowStyleManager::self(), SIGNAL(loadStylesFinished()), this, SLOT(slotLoadStyles()));


	// Since KNewStuff is incomplete and buggy we'll disable it by default.
	d->mPrfsChatWindow->btnGetStyles->setEnabled( config->readBoolEntry( "ForceNewStuff", false ) );

	connect(d->mPrfsChatWindow->mTransparencyTintColor, SIGNAL(activated (const QColor &)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsChatWindow->mTransparencyValue, SIGNAL(valueChanged(int)),
		this, SLOT(emitChanged()));

	d->mPrfsChatWindow->htmlFrame->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
	// Create the fake Chat Session
	createPreviewChatSession();
	QVBoxLayout *l = new QVBoxLayout(d->mPrfsChatWindow->htmlFrame);
	d->preview = new ChatMessagePart(d->previewChatSession, d->mPrfsChatWindow->htmlFrame, "preview");
	d->preview->setJScriptEnabled(false);
	d->preview->setJavaEnabled(false);
	d->preview->setPluginsEnabled(false);
	d->preview->setMetaRefreshEnabled(false);
	KHTMLView *htmlWidget = d->preview->view();
	htmlWidget->setMarginWidth(4);
	htmlWidget->setMarginHeight(4);
	htmlWidget->setFocusPolicy(NoFocus);
	htmlWidget->setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	l->addWidget(htmlWidget);
	// Add the preview message to the ChatMessagePart
	createPreviewMessages();

	d->mAppearanceTabCtl->addTab( d->mPrfsChatWindow, i18n("Chat Window") );

	// "Contact List" TAB =======================================================
	d->mPrfsContactList = new AppearanceConfig_ContactList(d->mAppearanceTabCtl);
	connect(d->mPrfsContactList->mTreeContactList, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mSortByGroup, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mEditTooltips, SIGNAL(clicked()),
		this, SLOT(slotEditTooltips()));
	connect(d->mPrfsContactList->mIndentContacts, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mHideVerticalScrollBar, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()) );
	connect(d->mPrfsContactList->mDisplayMode, SIGNAL(clicked(int)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mIconMode, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mAnimateChanges, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mFadeVisibility, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mFoldVisibility, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mAutoHide, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mAutoHideVScroll, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsContactList->mAutoHideTimeout, SIGNAL(valueChanged(int)),
		this, SLOT(emitChanged()));

	// don't enable the checkbox if XRender is not available
	#ifdef HAVE_XRENDER
	d->mPrfsContactList->mFadeVisibility->setEnabled(true);
	#else
	d->mPrfsContactList->mFadeVisibility->setEnabled(false);
	#endif

	d->mAppearanceTabCtl->addTab(d->mPrfsContactList, i18n("Contact List"));

	// "Colors and Fonts" TAB ===================================================
	d->mPrfsColors = new AppearanceConfig_Colors(d->mAppearanceTabCtl);
	connect(d->mPrfsColors->foregroundColor, SIGNAL(changed(const QColor &)),
		this, SLOT(slotHighlightChanged()));
	connect(d->mPrfsColors->backgroundColor, SIGNAL(changed(const QColor &)),
		this, SLOT(slotHighlightChanged()));
	connect(d->mPrfsColors->fontFace, SIGNAL(fontSelected(const QFont &)),
		this, SLOT(slotChangeFont()));
	connect(d->mPrfsColors->textColor, SIGNAL(changed(const QColor &)),
		this, SLOT(slotUpdatePreview()));
	connect(d->mPrfsColors->bgColor, SIGNAL(changed(const QColor &)),
		this, SLOT(slotUpdatePreview()));
	connect(d->mPrfsColors->linkColor, SIGNAL(changed(const QColor &)),
		this, SLOT(slotUpdatePreview()));
	connect(d->mPrfsColors->mGreyIdleMetaContacts, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsColors->idleContactColor, SIGNAL(changed(const QColor &)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsColors->mUseCustomFonts, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsColors->mSmallFont, SIGNAL(fontSelected(const QFont &)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsColors->mNormalFont, SIGNAL(fontSelected(const QFont &)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsColors->mGroupNameColor, SIGNAL(changed(const QColor &)),
		this, SLOT(emitChanged()));

	connect(d->mPrfsColors->mBgOverride, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsColors->mFgOverride, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));
	connect(d->mPrfsColors->mRtfOverride, SIGNAL(toggled(bool)),
		this, SLOT(emitChanged()));

	d->mAppearanceTabCtl->addTab(d->mPrfsColors, i18n("Colors && Fonts"));

	// ==========================================================================

	slotTransparencyChanged(d->mPrfsChatWindow->mTransparencyEnabled->isChecked());

	load();
}

AppearanceConfig::~AppearanceConfig()
{
	delete d;
}

void AppearanceConfig::save()
{
//	kdDebug(14000) << k_funcinfo << "called." << endl;
	KopetePrefs *p = KopetePrefs::prefs();

	// "Emoticons" TAB ==========================================================
	p->setIconTheme( d->mPrfsEmoticons->icon_theme_list->currentText() );
	p->setUseEmoticons ( d->mPrfsEmoticons->chkUseEmoticons->isChecked() );
	p->setEmoticonsRequireSpaces( d->mPrfsEmoticons->chkRequireSpaces->isChecked() );

	// "Chat Window" TAB ========================================================
	p->setTransparencyColor( d->mPrfsChatWindow->mTransparencyTintColor->color() );
	p->setTransparencyEnabled( d->mPrfsChatWindow->mTransparencyEnabled->isChecked() );
	p->setTransparencyValue( d->mPrfsChatWindow->mTransparencyValue->value() );

	// Get the stylePath
	if(d->currentStyle)
	{
		kdDebug(14000) << k_funcinfo << d->currentStyle->getStylePath() << endl;
		p->setStylePath(d->currentStyle->getStylePath());
	}
	// Get and save the styleVariant
	if( !d->currentVariantMap.empty() )
	{
		kdDebug(14000) << k_funcinfo << d->currentVariantMap[ d->mPrfsChatWindow->variantList->currentText()] << endl;
		p->setStyleVariant(d->currentVariantMap[ d->mPrfsChatWindow->variantList->currentText()]);
	}

	// "Contact List" TAB =======================================================
	p->setTreeView(d->mPrfsContactList->mTreeContactList->isChecked());
	p->setSortByGroup(d->mPrfsContactList->mSortByGroup->isChecked());
	p->setContactListIndentContacts(d->mPrfsContactList->mIndentContacts->isChecked());
	p->setContactListHideVerticalScrollBar(d->mPrfsContactList->mHideVerticalScrollBar->isChecked());
	p->setContactListDisplayMode(KopetePrefs::ContactDisplayMode(d->mPrfsContactList->mDisplayMode->selectedId()));
	p->setContactListIconMode(KopetePrefs::IconDisplayMode((d->mPrfsContactList->mIconMode->isChecked()) ? KopetePrefs::PhotoPic : KopetePrefs::IconPic));
	p->setContactListAnimation(d->mPrfsContactList->mAnimateChanges->isChecked());
	p->setContactListFading(d->mPrfsContactList->mFadeVisibility->isChecked());
	p->setContactListFolding(d->mPrfsContactList->mFoldVisibility->isChecked());

	// "Colors & Fonts" TAB =====================================================
	p->setHighlightBackground(d->mPrfsColors->backgroundColor->color());
	p->setHighlightForeground(d->mPrfsColors->foregroundColor->color());
	p->setBgColor(d->mPrfsColors->bgColor->color());
	p->setTextColor(d->mPrfsColors->textColor->color());
	p->setLinkColor(d->mPrfsColors->linkColor->color());
	p->setFontFace(d->mPrfsColors->fontFace->font());
	p->setIdleContactColor(d->mPrfsColors->idleContactColor->color());
	p->setGreyIdleMetaContacts(d->mPrfsColors->mGreyIdleMetaContacts->isChecked());
	p->setContactListUseCustomFonts(d->mPrfsColors->mUseCustomFonts->isChecked());
	p->setContactListCustomSmallFont(d->mPrfsColors->mSmallFont->font());
	p->setContactListCustomNormalFont(d->mPrfsColors->mNormalFont->font());
	p->setContactListGroupNameColor(d->mPrfsColors->mGroupNameColor->color());
	p->setContactListAutoHide(d->mPrfsContactList->mAutoHide->isChecked());
	p->setContactListAutoHideVScroll(d->mPrfsContactList->mAutoHideVScroll->isChecked());
	p->setContactListAutoHideTimeout(d->mPrfsContactList->mAutoHideTimeout->value());

	p->setBgOverride( d->mPrfsColors->mBgOverride->isChecked() );
	p->setFgOverride( d->mPrfsColors->mFgOverride->isChecked() );
	p->setRtfOverride( d->mPrfsColors->mRtfOverride->isChecked() );

	p->save();
	d->styleChanged = false;
}

void AppearanceConfig::load()
{
	//we will change the state of somme controls, which will call some signals.
	//so to don't refresh everything several times, we memorize we are loading.
	d->loading=true;

//	kdDebug(14000) << k_funcinfo << "called" << endl;
	KopetePrefs *p = KopetePrefs::prefs();

	// "Emoticons" TAB ==========================================================
	updateEmoticonlist();
	d->mPrfsEmoticons->chkUseEmoticons->setChecked( p->useEmoticons() );
	d->mPrfsEmoticons->chkRequireSpaces->setChecked( p->emoticonsRequireSpaces() );

	// "Chat Window" TAB ========================================================
	d->mPrfsChatWindow->mTransparencyEnabled->setChecked( p->transparencyEnabled() );
	d->mPrfsChatWindow->mTransparencyTintColor->setColor( p->transparencyColor() );
	d->mPrfsChatWindow->mTransparencyValue->setValue( p->transparencyValue() );
	// Look for avaiable chat window styles.
	ChatWindowStyleManager::self()->loadStyles();
	
	// "Contact List" TAB =======================================================
	d->mPrfsContactList->mTreeContactList->setChecked( p->treeView() );
	d->mPrfsContactList->mSortByGroup->setChecked( p->sortByGroup() );
	d->mPrfsContactList->mIndentContacts->setChecked( p->contactListIndentContacts() );
	d->mPrfsContactList->mHideVerticalScrollBar->setChecked( p->contactListHideVerticalScrollBar() );

        // convert old single value display mode to dual display/icon modes
        if (p->contactListDisplayMode() == KopetePrefs::Yagami) {
            	p->setContactListDisplayMode( KopetePrefs::Detailed);
            	p->setContactListIconMode( KopetePrefs::PhotoPic );
        }

	d->mPrfsContactList->mDisplayMode->setButton( p->contactListDisplayMode() );
	d->mPrfsContactList->mIconMode->setChecked( p->contactListIconMode() == KopetePrefs::PhotoPic);

            
	d->mPrfsContactList->mAnimateChanges->setChecked( p->contactListAnimation() );
#ifdef HAVE_XRENDER
	d->mPrfsContactList->mFadeVisibility->setChecked( p->contactListFading() );
#else
	d->mPrfsContactList->mFadeVisibility->setChecked( false );
#endif
	d->mPrfsContactList->mFoldVisibility->setChecked( p->contactListFolding() );
	d->mPrfsContactList->mAutoHide->setChecked( p->contactListAutoHide() );
	d->mPrfsContactList->mAutoHideVScroll->setChecked( p->contactListAutoHideVScroll() );
	d->mPrfsContactList->mAutoHideTimeout->setValue( p->contactListAutoHideTimeout() );

	// "Colors & Fonts" TAB =====================================================
	d->mPrfsColors->foregroundColor->setColor(p->highlightForeground());
	d->mPrfsColors->backgroundColor->setColor(p->highlightBackground());
	d->mPrfsColors->textColor->setColor(p->textColor());
	d->mPrfsColors->linkColor->setColor(p->linkColor());
	d->mPrfsColors->bgColor->setColor(p->bgColor());
	d->mPrfsColors->fontFace->setFont(p->fontFace());
	d->mPrfsColors->mGreyIdleMetaContacts->setChecked(p->greyIdleMetaContacts());
	d->mPrfsColors->idleContactColor->setColor(p->idleContactColor());
	d->mPrfsColors->mUseCustomFonts->setChecked(p->contactListUseCustomFonts());
	d->mPrfsColors->mSmallFont->setFont(p->contactListCustomSmallFont());
	d->mPrfsColors->mNormalFont->setFont(p->contactListCustomNormalFont());
	d->mPrfsColors->mGroupNameColor->setColor(p->contactListGroupNameColor());

	d->mPrfsColors->mBgOverride->setChecked( p->bgOverride() );
	d->mPrfsColors->mFgOverride->setChecked( p->fgOverride() );
	d->mPrfsColors->mRtfOverride->setChecked( p->rtfOverride() );

	d->loading=false;
	slotUpdatePreview();
}

void AppearanceConfig::slotLoadStyles()
{
	d->mPrfsChatWindow->styleList->clear();
	d->styleItemMap.clear();

	ChatWindowStyleManager::StyleList availableStyles;
	availableStyles = ChatWindowStyleManager::self()->getAvailableStyles();
	if( availableStyles.empty() )
		kdDebug(14000) << k_funcinfo << "Warning, available styles is empty !" << endl;

	ChatWindowStyleManager::StyleList::ConstIterator it, itEnd = availableStyles.constEnd();
	for(it = availableStyles.constBegin(); it != itEnd; ++it)
	{
		// Insert style name into the listbox
		d->mPrfsChatWindow->styleList->insertItem( it.key(), 0 );
		// Insert the style class into the internal map for futher acces.
		d->styleItemMap.insert( d->mPrfsChatWindow->styleList->firstItem(), it.data() );

		if( it.data()->getStylePath() == KopetePrefs::prefs()->stylePath() )
		{
			kdDebug(14000) << k_funcinfo << "Restoring saved style: " << it.key() << endl;

			d->currentStyle = it.data();
			d->mPrfsChatWindow->styleList->setSelected( d->mPrfsChatWindow->styleList->firstItem(), true );
		}
	}
	if(d->currentStyle)
	{
		// Set the initial preview style
		d->preview->setStyle(d->currentStyle);
		d->preview->setStyleVariant(KopetePrefs::prefs()->styleVariant());
	}

	d->mPrfsChatWindow->styleList->sort();
}

void AppearanceConfig::updateEmoticonlist()
{
	KopetePrefs *p = KopetePrefs::prefs();
	KStandardDirs dir;

	d->mPrfsEmoticons->icon_theme_list->clear(); // Wipe out old list
	// Get a list of directories in our icon theme dir
	QStringList themeDirs = KGlobal::dirs()->findDirs("emoticons", "");
	// loop adding themes from all dirs into theme-list
	for(unsigned int x = 0;x < themeDirs.count();x++)
	{
		QDir themeQDir(themeDirs[x]);
		themeQDir.setFilter( QDir::Dirs ); // only scan for subdirs
		themeQDir.setSorting( QDir::Name ); // I guess name is as good as any
		for(unsigned int y = 0; y < themeQDir.count(); y++)
		{
			QStringList themes = themeQDir.entryList(QDir::Dirs, QDir::Name);
			// We don't care for '.' and '..'
			if ( themeQDir[y] != "." && themeQDir[y] != ".." )
			{
				// Add ourselves to the list, using our directory name  FIXME:  use the first emoticon of the theme.
				QPixmap previewPixmap = QPixmap(locate("emoticons", themeQDir[y]+"/smile.png"));
				d->mPrfsEmoticons->icon_theme_list->insertItem(previewPixmap,themeQDir[y]);
			}
		}
	}

	// Where is that theme in our big-list-o-themes?
	QListBoxItem *item = d->mPrfsEmoticons->icon_theme_list->findItem( p->iconTheme() );

	if (item) // found it... make it the currently selected theme
		d->mPrfsEmoticons->icon_theme_list->setCurrentItem( item );
	else // Er, it's not there... select the current item
		d->mPrfsEmoticons->icon_theme_list->setCurrentItem( 0 );
}

void AppearanceConfig::slotSelectedEmoticonsThemeChanged()
{
	QString themeName = d->mPrfsEmoticons->icon_theme_list->currentText();
	QFileInfo fileInf(KGlobal::dirs()->findResource("emoticons", themeName+"/"));
	d->mPrfsEmoticons->btnRemoveTheme->setEnabled( fileInf.isWritable() );

	Kopete::Emoticons emoticons( themeName );
	QStringList smileys = emoticons.emoticonAndPicList().values();
	QString newContentText = "<qt>";

	for(QStringList::Iterator it = smileys.begin(); it != smileys.end(); ++it )
		newContentText += QString::fromLatin1("<img src=\"%1\"> ").arg(*it);

	newContentText += QString::fromLatin1("</qt>");
	d->mPrfsEmoticons->icon_theme_preview->setText(newContentText);
	emitChanged();
}

void AppearanceConfig::slotTransparencyChanged ( bool checked )
{
	d->mPrfsChatWindow->mTransparencyTintColor->setEnabled( checked );
	d->mPrfsChatWindow->mTransparencyValue->setEnabled( checked );
	emitChanged();
}

void AppearanceConfig::slotHighlightChanged()
{
//	bool value = mPrfsChatWindow->highlightEnabled->isChecked();
//	mPrfsChatWindow->foregroundColor->setEnabled ( value );
//	mPrfsChatWindow->backgroundColor->setEnabled ( value );
	slotUpdatePreview();
}

void AppearanceConfig::slotChangeFont()
{
	slotUpdatePreview();
	emitChanged();
}

void AppearanceConfig::slotStyleSelected()
{
	// Retrieve variant list.
	d->currentStyle = d->styleItemMap[d->mPrfsChatWindow->styleList->selectedItem()];
	
	if(d->currentStyle)
	{
		d->currentVariantMap = d->currentStyle->getVariants();
		kdDebug(14000) << "Loading style: " << d->currentStyle->getStylePath() << endl;
	
		// Update the variant list based on current style.
		d->mPrfsChatWindow->variantList->clear();
	
		ChatWindowStyle::StyleVariants::ConstIterator it, itEnd = d->currentVariantMap.constEnd();
		int currentIndex = 0;
		for(it = d->currentVariantMap.constBegin(); it != itEnd; ++it)
		{
			// Insert variant name into the combobox.
			d->mPrfsChatWindow->variantList->insertItem( it.key() );
	
			if( it.data() == KopetePrefs::prefs()->styleVariant() )
				d->mPrfsChatWindow->variantList->setCurrentItem(currentIndex);
	
			currentIndex++;
		}
		
		// Update the preview
		d->preview->setStyle(d->currentStyle);
		// Get the first variant to preview
		// Check if the current style has variants.
		if( !d->currentVariantMap.empty() )
			d->preview->setStyleVariant(d->currentVariantMap[0]);
	
		emitChanged();
	}
}

void AppearanceConfig::slotVariantSelected(const QString &variantName)
{
// 	kdDebug(14000) << k_funcinfo << variantName << endl;
// 	kdDebug(14000) << k_funcinfo << d->currentVariantMap[variantName] << endl;

	// Update the preview
	d->preview->setStyleVariant(d->currentVariantMap[variantName]);
	emitChanged();
}

void AppearanceConfig::slotImportStyle()
{
	KURL chosenStyle = KURLRequesterDlg::getURL( QString::null, this, i18n( "Choose Stylesheet" ) );
	if ( !chosenStyle.isEmpty() )
	{
		QString stylePath;
		// FIXME: Using NetAccess uses nested event loops with all associated problems.
		//        Better use normal KIO and an async API - Martijn
		if ( KIO::NetAccess::download( chosenStyle, stylePath, this ) )
		{
// 			QString styleSheet = fileContents( stylePath );
// 			if ( Kopete::XSLT( styleSheet ).isValid() )
// 			{
// 				QFileInfo fi( stylePath );
// 				addStyle( fi.fileName().section( '.', 0, 0 ), styleSheet );
// 			}
// 			else
// 			{
// 				KMessageBox::queuedMessageBox( this, KMessageBox::Error,
// 					i18n( "'%1' is not a valid XSLT document. Import canceled." ).arg( chosenStyle.path() ),
// 					i18n( "Invalid Style" ) );
// 			}
		}
		else
		{
			KMessageBox::queuedMessageBox( this, KMessageBox::Error,
				i18n( "Could not import '%1'. Check access permissions/network connection." ).arg( chosenStyle.path() ),
				i18n( "Import Error" ) );
		}
	}
}

void AppearanceConfig::slotCopyStyle()
{
// 	QListBoxItem *copiedItem = d->mPrfsChatWindow->styleList->selectedItem();
// 	if( copiedItem )
// 	{
// 		QString styleName =
// 			KInputDialog::getText( i18n( "New Style Name" ), i18n( "Enter the name of the new style:" ) );
// 
// 		if ( !styleName.isEmpty() )
// 		{
// 			QString stylePath = d->itemMap[ copiedItem ];
// 			addStyle( styleName, fileContents( stylePath ) );
// 		}
// 	}
// 	else
// 	{
// 		KMessageBox::queuedMessageBox( this, KMessageBox::Sorry,
// 			i18n("Please select a style to copy."), i18n("No Style Selected") );
// 	}
	emitChanged();
}

void AppearanceConfig::slotDeleteStyle()
{
// 	if( KMessageBox::warningContinueCancel( this, i18n("Are you sure you want to delete the style \"%1\"?")
// 		.arg( d->mPrfsChatWindow->styleList->selectedItem()->text() ),
// 		i18n("Delete Style"), KGuiItem(i18n("Delete Style"),"editdelete")) == KMessageBox::Continue )
// 	{
// 		QListBoxItem *style = d->mPrfsChatWindow->styleList->selectedItem();
// 		QString filePath = d->itemMap[ style ];
// 		d->itemMap.remove( style );
// 
// 		QFileInfo fi( filePath );
// 		if( fi.isWritable() )
// 			QFile::remove( filePath );
// 
// 		KConfig *config = KGlobal::config();
// 		config->setGroup("KNewStuffStatus");
// 		config->deleteEntry( style->text() );
// 		config->sync();
// 
// 		if( style->next() )
// 			d->mPrfsChatWindow->styleList->setSelected( style->next(), true );
// 		else
// 			d->mPrfsChatWindow->styleList->setSelected( style->prev(), true );
// 		delete style;
// 	}
	emitChanged();
}

void AppearanceConfig::slotGetStyles()
{
	// we need this because KNewStuffGeneric's install function isn't clever enough
	KopeteStyleNewStuff * kns = new KopeteStyleNewStuff( "kopete/chatstyle", this );
	KNS::Engine * engine = new KNS::Engine( kns, "kopete/chatstyle", this );
	KNS::DownloadDialog * d = new KNS::DownloadDialog( engine, this );
	d->setType( "kopete/chatstyle" );
	// you have to do this by hand when providing your own Engine
	KNS::ProviderLoader * p = new KNS::ProviderLoader( this );
	QObject::connect( p, SIGNAL( providersLoaded(Provider::List*) ), d, SLOT( slotProviders (Provider::List *) ) );
	p->load( "kopete/chatstyle", "http://download.kde.org/khotnewstuff/kopetestyles-providers.xml" );
	d->exec();
}

// Reimplement Kopete::Contact and its abstract method
// This is for style preview.
class FakeContact : public Kopete::Contact
{
public:
	FakeContact (Kopete::Account *account, const QString &id, Kopete::MetaContact *mc ) : Kopete::Contact( account, id, mc ) {}
	virtual Kopete::ChatSession *manager(Kopete::Contact::CanCreateFlags /*c*/) { return 0L; }
	virtual void slotUserInfo() {};
};

// This is for style preview.
class FakeProtocol : public Kopete::Protocol
{
public:
FakeProtocol( KInstance *instance, QObject *parent, const char *name ) : Kopete::Protocol(instance, parent, name){}
Kopete::Account* createNewAccount( const QString &/*accountId*/ ){return 0L;}
AddContactPage* createAddContactWidget( QWidget */*parent*/, Kopete::Account */*account*/){return 0L;}
KopeteEditAccountWidget* createEditAccountWidget( Kopete::Account */*account*/, QWidget */*parent */){return 0L;}
};

// This is for style preview.
class FakeAccount : public Kopete::Account
{
public:
FakeAccount(Kopete::Protocol *parent, const QString &accountID, const char *name) : Kopete::Account(parent, accountID, name){}
~FakeAccount()
{}
bool createContact( const QString &/*contactId*/, Kopete::MetaContact */*parentContact*/ ){return true;}
void connect( const Kopete::OnlineStatus& /*initialStatus*/){}
void disconnect(){}
void setOnlineStatus( const Kopete::OnlineStatus& /*status*/ , const QString &/*reason*/){}
};

void AppearanceConfig::createPreviewChatSession()
{
	d->previewProtocol = new FakeProtocol( new KInstance(QCString("kopete-preview-chatwindowstyle")), 0L, "kopete-preview-chatwindowstyle");
	d->previewAccount = new FakeAccount(d->previewProtocol, QString("previewaccount"), 0);

	// Create fake meta/contacts
	d->myselfMetaContact = new Kopete::MetaContact();
	d->myself = new FakeContact(d->previewAccount, i18n("This is the myself preview contact id", "myself@preview"), d->myselfMetaContact);
	d->myself->setNickName(i18n("This is the myself preview contact nickname", "Myself"));
	d->jackMetaContact = new Kopete::MetaContact();
	d->jack = new FakeContact(d->previewAccount, i18n("This is the other preview contact id", "jack@preview"), d->jackMetaContact);
	d->jack->setNickName(i18n("This is the other preview contact nickname", "Jack"));
	d->myselfMetaContact->setDisplayName(i18n("Myself"));
	d->myselfMetaContact->setDisplayNameSource(Kopete::MetaContact::SourceCustom);
	d->jackMetaContact->setDisplayName(i18n("Jack"));
	d->jackMetaContact->setDisplayNameSource(Kopete::MetaContact::SourceCustom);

	Kopete::ContactPtrList contactList;
	contactList.append(d->jack);
	// Create fakeChatSession
	d->previewChatSession = Kopete::ChatSessionManager::self()->create(d->myself, contactList, 0);
	d->previewChatSession->setDisplayName("Preview Session");	
}

void AppearanceConfig::createPreviewMessages()
{
	Kopete::Message msgIn( d->jack,d->myself, i18n( "Hello, this is an incoming message :-)" ), Kopete::Message::Inbound );
	Kopete::Message msgIn2( d->jack, d->myself, i18n( "Hello, this a incoming consecutive message." ), Kopete::Message::Inbound );

	Kopete::Message msgOut( d->myself, d->jack, i18n( "Ok, this is an outgoing message" ), Kopete::Message::Outbound );
	Kopete::Message msgOut2( d->myself, d->jack, i18n( "Ok, a outgoing consecutive message." ), Kopete::Message::Outbound );
 
	Kopete::Message msgCol( d->jack, d->myself, i18n( "Here is an incoming colored message" ), Kopete::Message::Inbound );
	msgCol.setFg( QColor( "DodgerBlue" ) );
	msgCol.setBg( QColor( "LightSteelBlue" ) );
	Kopete::Message msgInt( d->jack, d->myself, i18n( "This is an internal message" ), Kopete::Message::Internal );
	Kopete::Message msgAct( d->jack, d->myself, i18n( "performed an action" ), Kopete::Message::Inbound,
				  Kopete::Message::PlainText, QString::null, Kopete::Message::TypeAction );
	Kopete::Message msgHigh( d->jack, d->myself, i18n( "This is a highlighted message" ), Kopete::Message::Inbound );
	msgHigh.setImportance( Kopete::Message::Highlight );
	Kopete::Message msgBye ( d->myself, d->jack,   i18n( "Bye" ), Kopete::Message::Outbound );

	// Add the messages to ChatMessagePart
	d->preview->appendMessage(msgIn);
	d->preview->appendMessage(msgIn2);
	d->preview->appendMessage(msgOut);
	d->preview->appendMessage(msgOut2);
	d->preview->appendMessage(msgCol);
	d->preview->appendMessage(msgInt);
	d->preview->appendMessage(msgAct);
	d->preview->appendMessage(msgHigh);
	d->preview->appendMessage(msgBye);
}

void AppearanceConfig::slotUpdatePreview()
{
	if(d->loading)
		return;

// 	QListBoxItem *style = d->mPrfsChatWindow->styleList->selectedItem();
// 	if( style && style->text() != d->currentStyle )
// 	{
		//TODO: should be using a ChatMessagePart
// 		d->preview->begin();
// 		d->preview->write( QString::fromLatin1(
// 			"<html><head><style>"
// 			"body{ font-family:%1;color:%2; }"
// 			"td{ font-family:%3;color:%4; }"
// 			".highlight{ color:%5;background-color:%6 }"
// 			"</style></head>"
// 			"<body bgcolor=\"%7\" vlink=\"%8\" link=\"%9\">"
// 		).arg( mPrfsColors->fontFace->font().family() ).arg( mPrfsColors->textColor->color().name() )
// 			.arg( mPrfsColors->fontFace->font().family() ).arg( mPrfsColors->textColor->color().name() )
// 			.arg( mPrfsColors->foregroundColor->color().name() ).arg( mPrfsColors->backgroundColor->color().name() )
// 			.arg( mPrfsColors->bgColor->color().name() ).arg( mPrfsColors->linkColor->color().name() )
// 			.arg( mPrfsColors->linkColor->color().name() ) );

// 		QString stylePath = d->itemMap[ style ];
// 		//d->xsltParser->setXSLT( fileContents(stylePath) );
// 		//preview->write( d->xsltParser->transform( sampleConversationXML() ) );
// 		preview->write( QString::fromLatin1( "</body></html>" ) );
// 		preview->end();

		emitChanged();
// 	}
}

void AppearanceConfig::emitChanged()
{
	emit changed( true );
}

void AppearanceConfig::installNewTheme()
{
	KURL themeURL = KURLRequesterDlg::getURL(QString::null, this,
			i18n("Drag or Type Emoticon Theme URL"));
	if ( themeURL.isEmpty() )
		return;

	//TODO: support remote theme files!
	if ( !themeURL.isLocalFile() )
	{
		KMessageBox::queuedMessageBox( this, KMessageBox::Error, i18n("Sorry, emoticon themes must be installed from local files."),
		                               i18n("Could Not Install Emoticon Theme") );
		return;
	}

	Kopete::Global::installEmoticonTheme( themeURL.path() );
	updateEmoticonlist();
}

void AppearanceConfig::removeSelectedTheme()
{
	QListBoxItem *selected = d->mPrfsEmoticons->icon_theme_list->selectedItem();
	if(selected==0)
		return;

	QString themeName = selected->text();

	QString question=i18n("<qt>Are you sure you want to remove the "
			"<strong>%1</strong> emoticon theme?<br>"
			"<br>"
			"This will delete the files installed by this theme.</qt>").
		arg(themeName);

        int res = KMessageBox::warningContinueCancel(this, question, i18n("Confirmation"),KStdGuiItem::del());
	if (res!=KMessageBox::Continue)
		return;

	KURL themeUrl(KGlobal::dirs()->findResource("emoticons", themeName+"/"));
	KIO::NetAccess::del(themeUrl, this);

	updateEmoticonlist();
}

void AppearanceConfig::slotGetThemes()
{
	KConfig* config = KGlobal::config();
	config->setGroup( "KNewStuff" );
	config->writeEntry( "ProvidersUrl",
						"http://download.kde.org/khotnewstuff/emoticons-providers.xml" );
	config->writeEntry( "StandardResource", "emoticons" );
	config->writeEntry( "Uncompress", "application/x-gzip" );
	config->sync();
	
	KNS::DownloadDialog::open( "emoticons", i18n( "Get New Emoticons") );
	updateEmoticonlist();
}

void AppearanceConfig::slotEditTooltips()
{
	TooltipEditDialog *dlg = new TooltipEditDialog(this);
	connect(dlg, SIGNAL(changed(bool)), this, SIGNAL(changed(bool)));
	dlg->exec();
	delete dlg;
}

#include "appearanceconfig.moc"
// vim: set noet ts=4 sts=4 sw=4:
