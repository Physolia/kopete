// -*- Mode: c++-mode; c-basic-offset: 2; indent-tabs-mode: t; tab-width: 2; -*-
//
// Copyright (C) 2004 Grzegorz Jaskiewicz <gj at pointblue.com.pl>
//
// gadudcctransaction.cpp
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


#ifndef GADUDCCTRANS_H
#define GADUDCCTRANS_H

#include <qobject.h>
#include <qfile.h>

class QSocketNotifier;
class gg_dcc;
class GaduAccount;
class GaduContact;
namespace Kopete { class Transfer; }
namespace Kopete { class FileTransferInfo; }
class GaduDCC;

class GaduDCCTransaction: QObject {
	Q_OBJECT
public:
	GaduDCCTransaction( GaduDCC*, const char* name = NULL );
	~GaduDCCTransaction();

	bool setupIncoming( const unsigned int, GaduContact* );
	bool setupIncoming( gg_dcc* );
	unsigned int recvUIN();
	unsigned int peerUIN();

public slots:

signals:

protected:

protected slots:

private slots:
	void watcher();
	void slotIncomingTransferAccepted ( Kopete::Transfer*, const QString& );
	void slotTransferRefused ( const Kopete::FileTransferInfo& );
	void slotTransferResult();

private:
	void enableNotifiers( int );
	void disableNotifiers();
	void checkDescriptor();
	void closeDCC();
	void destroyNotifiers();
	void createNotifiers( bool );
	void askIncommingTransfer();

	gg_dcc* dccSock_;

	QSocketNotifier* read_;
	QSocketNotifier* write_;

	GaduContact* contact;

	Kopete::Transfer* transfer_;
	long transferId_;
	QFile localFile_;
	bool peer;
	unsigned int incoming;
	GaduDCC* gaduDCC_;
};

#endif
