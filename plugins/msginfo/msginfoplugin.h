////////////////////////////////////////////////////////////////////////////////
// msginfoplugin.h                                                            //
//                                                                            //
// Copyright (C)  2002  Zack Rusin <zack@kde.org>                             //
//                                                                            //
// This library is free software; you can redistribute it and/or              //
// modify it under the terms of the GNU Lesser General Public                 //
// License as published by the Free Software Foundation; either               //
// version 2.1 of the License, or (at your option) any later version.         //
//                                                                            //
// This library is distributed in the hope that it will be useful,            //
// but WITHOUT ANY WARRANTY; without even the implied warranty of             //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU          //
// Lesser General Public License for more details.                            //
//                                                                            //
// You should have received a copy of the GNU Lesser General Public           //
// License along with this library; if not, write to the Free Software        //
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA                   //
// 02111-1307  USA                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef AUTOAWAYPLUGIN_H
#define AUTOAWAYPLUGIN_H

#include <qobject.h>
#include <qmap.h>

#include "kopetemessage.h"
#include "kopeteplugin.h"

class QStringList;
class KopeteMessage;
class KopeteMetaContact;

class MsgInfoPlugin : public KopetePlugin
{
	Q_OBJECT
public:
	MsgInfoPlugin( QObject *parent, const char *name, const QStringList &args );
	~MsgInfoPlugin();

	void init();
	bool unload();

	bool serialize( KopeteMetaContact *metaContact,
			QStringList &strList) const;
	void deserialize( KopeteMetaContact *metaContact, const QStringList& data );

public slots:
	void slotProcessDisplay( KopeteMessage& msg );
	void slotProcessSend( KopeteMessage& msg );

protected:
	void changeMessage( KopeteMessage& msg );
private:
	QMap<const KopeteMetaContact*, Q_UINT32> mMsgCountMap;
};

#endif


