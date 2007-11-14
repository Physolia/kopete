 /*
    kopetechatwindowstyle.cpp - A Chat Window Style.

    Copyright (c) 2005      by Michaël Larouche     <larouche@kde.org>

    Kopete    (c) 2002-2005 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "kopetechatwindowstyle.h"

// Qt includes
#include <QFile>
#include <QDir>
#include <QHash>
#include <QStringList>
#include <QTextStream>

// KDE includes
#include <kdebug.h>
#include <kstandarddirs.h>

class ChatWindowStyle::Private
{
public:
	QString styleName;
	StyleVariants variantsList;
	QString baseHref;
	QString currentVariantPath;

	QString headerHtml;
	QString footerHtml;
	QString incomingHtml;
	QString nextIncomingHtml;
	QString outgoingHtml;
	QString nextOutgoingHtml;
	QString statusHtml;
	QString actionIncomingHtml;
	QString actionOutgoingHtml;
	QHash<QString, bool> compactVariants;
};

ChatWindowStyle::ChatWindowStyle(const QString &styleName, StyleBuildMode styleBuildMode)
	: d(new Private)
{
	init(styleName, styleBuildMode);
}

ChatWindowStyle::ChatWindowStyle(const QString &styleName, const QString &variantPath, StyleBuildMode styleBuildMode)
	: d(new Private)
{
	d->currentVariantPath = variantPath;
	init(styleName, styleBuildMode);
}

void ChatWindowStyle::init(const QString &styleName, StyleBuildMode styleBuildMode)
{
	QStringList styleDirs = KGlobal::dirs()->findDirs("appdata", QString("styles/%1/Contents/Resources/").arg(styleName));
	if(styleDirs.isEmpty())
	{
		kDebug(14000) << "Failed to find style" << styleName;
		return;
	}
	d->styleName = styleName;
	if(styleDirs.count() > 1)
		kDebug(14000) << "found several styles with the same name. using first";
	d->baseHref = styleDirs.at(0);
	kDebug(14000) << "Using style:" << d->baseHref;
	readStyleFiles();
	if(styleBuildMode & StyleBuildNormal)
	{
		listVariants();
	}
}

ChatWindowStyle::~ChatWindowStyle()
{
	kDebug(14000) ;
	delete d;
}

ChatWindowStyle::StyleVariants ChatWindowStyle::getVariants()
{
	// If the variantList is empty, list available variants.
	if( d->variantsList.isEmpty() )
	{
		listVariants();
	}
	return d->variantsList;
}

QString ChatWindowStyle::getStyleName() const
{
	return d->styleName;
}

QString ChatWindowStyle::getStyleBaseHref() const
{
	return d->baseHref;
}

QString ChatWindowStyle::getHeaderHtml() const
{
	return d->headerHtml;
}

QString ChatWindowStyle::getFooterHtml() const
{
	return d->footerHtml;
}

QString ChatWindowStyle::getIncomingHtml() const
{
	return d->incomingHtml;
}

QString ChatWindowStyle::getNextIncomingHtml() const
{
	return d->nextIncomingHtml;
}

QString ChatWindowStyle::getOutgoingHtml() const
{
	return d->outgoingHtml;
}

QString ChatWindowStyle::getNextOutgoingHtml() const
{
	return d->nextOutgoingHtml;
}

QString ChatWindowStyle::getStatusHtml() const
{
	return d->statusHtml;
}

QString ChatWindowStyle::getActionIncomingHtml() const
{
	return d->actionIncomingHtml;	
}

QString ChatWindowStyle::getActionOutgoingHtml() const
{
	return d->actionOutgoingHtml;
}

bool ChatWindowStyle::hasActionTemplate() const
{
	return ( !d->actionIncomingHtml.isEmpty() && !d->actionOutgoingHtml.isEmpty() );
}

void ChatWindowStyle::listVariants()
{
	QString variantDirPath = d->baseHref + QString::fromUtf8("Variants/");
	QDir variantDir(variantDirPath);

	QStringList variantList = variantDir.entryList( QStringList("*.css") );
	QStringList::ConstIterator it, itEnd = variantList.constEnd();
	QLatin1String compactVersionPrefix("_compact_");
	for(it = variantList.constBegin(); it != itEnd; ++it)
	{
		QString variantName = *it, variantPath;
		// Retrieve only the file name.
		variantName = variantName.left(variantName.lastIndexOf("."));
		if ( variantName.startsWith( compactVersionPrefix ) ) {
			if ( variantName == compactVersionPrefix ) {
				d->compactVariants.insert( "", true );
			}
			continue;
		}
		QString compactVersionFilename = *it;
		QString compactVersionPath = variantDirPath + compactVersionFilename.prepend( compactVersionPrefix );
		if ( QFile::exists( compactVersionPath )) {
			d->compactVariants.insert( variantName, true );
		}
		// variantPath is relative to baseHref.
		variantPath = QString("Variants/%1").arg(*it);
		d->variantsList.insert(variantName, variantPath);
	}
}

void ChatWindowStyle::readStyleFiles()
{
	QString headerFile = d->baseHref + QString("Header.html");
	QString footerFile = d->baseHref + QString("Footer.html");
	QString incomingFile = d->baseHref + QString("Incoming/Content.html");
	QString nextIncomingFile = d->baseHref + QString("Incoming/NextContent.html");
	QString outgoingFile = d->baseHref + QString("Outgoing/Content.html");
	QString nextOutgoingFile = d->baseHref + QString("Outgoing/NextContent.html");
	QString statusFile = d->baseHref + QString("Status.html");
	QString actionIncomingFile = d->baseHref + QString("Incoming/Action.html");
	QString actionOutgoingFile = d->baseHref + QString("Outgoing/Action.html");

	QFile fileAccess;
	// First load header file.
	if( QFile::exists(headerFile) )
	{
		fileAccess.setFileName(headerFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->headerHtml = headerStream.readAll();
		kDebug(14000) << "Header HTML: " << d->headerHtml;
		fileAccess.close();
	}
	// Load Footer file
	if( QFile::exists(footerFile) )
	{
		fileAccess.setFileName(footerFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->footerHtml = headerStream.readAll();
		kDebug(14000) << "Footer HTML: " << d->footerHtml;
		fileAccess.close();
	}
	// Load incoming file
	if( QFile::exists(incomingFile) )
	{
		fileAccess.setFileName(incomingFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->incomingHtml = headerStream.readAll();
		kDebug(14000) << "Incoming HTML: " << d->incomingHtml;
		fileAccess.close();
	}
	// Load next Incoming file
	if( QFile::exists(nextIncomingFile) )
	{
		fileAccess.setFileName(nextIncomingFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->nextIncomingHtml = headerStream.readAll();
		kDebug(14000) << "NextIncoming HTML: " << d->nextIncomingHtml;
		fileAccess.close();
	}
	// Load outgoing file
	if( QFile::exists(outgoingFile) )
	{
		fileAccess.setFileName(outgoingFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->outgoingHtml = headerStream.readAll();
		kDebug(14000) << "Outgoing HTML: " << d->outgoingHtml;
		fileAccess.close();
	}
	// Load next outgoing file
	if( QFile::exists(nextOutgoingFile) )
	{
		fileAccess.setFileName(nextOutgoingFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->nextOutgoingHtml = headerStream.readAll();
		kDebug(14000) << "NextOutgoing HTML: " << d->nextOutgoingHtml;
		fileAccess.close();
	}
	// Load status file
	if( QFile::exists(statusFile) )
	{
		fileAccess.setFileName(statusFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->statusHtml = headerStream.readAll();
		kDebug(14000) << "Status HTML: " << d->statusHtml;
		fileAccess.close();
	}
	
	// Load Action Incoming file
	if( QFile::exists(actionIncomingFile) )
	{
		fileAccess.setFileName(actionIncomingFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->actionIncomingHtml = headerStream.readAll();
		kDebug(14000) << "ActionIncoming HTML: " << d->actionIncomingHtml;
		fileAccess.close();
	}
	// Load Action Outgoing file
	if( QFile::exists(actionOutgoingFile) )
	{
		fileAccess.setFileName(actionOutgoingFile);
		fileAccess.open(QIODevice::ReadOnly);
		QTextStream headerStream(&fileAccess);
		headerStream.setCodec(QTextCodec::codecForName("UTF-8"));
		d->actionOutgoingHtml = headerStream.readAll();
		kDebug(14000) << "ActionOutgoing HTML: " << d->actionOutgoingHtml;
		fileAccess.close();
	}
}

void ChatWindowStyle::reload()
{
	d->variantsList.clear();
	readStyleFiles();
	listVariants();
}

bool ChatWindowStyle::hasCompact( const QString & styleVariant ) const
{
	if ( d->compactVariants.contains( styleVariant ) ) {
		return d->compactVariants.value( styleVariant );
	}
	return false;
}

QString ChatWindowStyle::compact( const QString & styleVariant ) const
{
	QString compacted = styleVariant;
	if ( styleVariant.isEmpty() ) {
		return QLatin1String( "Variants/_compact_.css" );
	} else {
		return compacted.insert( compacted.lastIndexOf('/') + 1, QString("_compact_") );
	}
}
