// filetransfertask.cpp

// Copyright (C)  2006

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301  USA

#include "oscarsettings.h"
#include "filetransfertask.h"

#include <ksocketaddress.h>
#include <kserversocket.h>
#include <kbufferedsocket.h>
#include <krandom.h>
#include <qstring.h>
#include <kdebug.h>
#include "buffer.h"
#include "connection.h"
#include "ofttransfer.h"
#include "oftprotocol.h"
#include "oscarutils.h"
#include <typeinfo>
#include "kopetetransfermanager.h"
#include <qfileinfo.h>
#include <klocale.h>

//TODO: don't have such ugly constructors

//receive
FileTransferTask::FileTransferTask( Task* parent, const QString& contact, const QString& self, QByteArray cookie, Buffer b  )
:Task( parent ), m_action( Receive ), m_file( this ), m_contactName( contact ), m_selfName( self ), m_cookie( cookie ), m_ss(0), m_connection(0), m_timer( this ), m_size( 0 ), m_bytes( 0 ), m_port( 0 ), m_proxy( 0 ), m_proxyRequester( 0 ), m_state( Default )
{
	parseReq( b );
	
}

//send
FileTransferTask::FileTransferTask( Task* parent, const QString& contact, const QString& self, const QString &fileName, Kopete::Transfer *transfer )
:Task( parent ), m_action( Send ), m_file( fileName, this ), m_contactName( contact ), m_selfName( self ), m_ss(0), m_connection(0), m_timer( this ), m_bytes( 0 ), m_port( 0 ), m_proxy( 0 ), m_proxyRequester( 0 ), m_state( Default )
{
	//get filename without path
	m_name = QFileInfo( fileName ).fileName();
	//copy size for convenience
	m_size = m_file.size();
	//we get to make up an icbm cookie!
	Buffer b;
	DWORD cookie = KRandom::random();
	b.addDWord( cookie );
	cookie = KRandom::random();
	b.addDWord( cookie );
	m_cookie = b.buffer();

	//hook up ui cancel
	connect( transfer , SIGNAL(transferCanceled()), this, SLOT( doCancel() ) );
	//hook up our ui signals
	connect( this , SIGNAL( gotCancel() ), transfer, SLOT( slotCancelled() ) );
	connect( this , SIGNAL( gotAccept() ), transfer, SLOT( slotAccepted() ) );
	connect( this , SIGNAL( error( int, const QString & ) ), transfer, SLOT( slotError( int, const QString & ) ) );
	connect( this , SIGNAL( processed( unsigned int ) ), transfer, SLOT( slotProcessed( unsigned int ) ) );
	connect( this , SIGNAL( fileComplete() ), transfer, SLOT( slotComplete() ) );
}

FileTransferTask::~FileTransferTask()
{
	if( m_connection )
	{
		delete m_connection;
		m_connection = 0;
	}
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "done" << endl;
}

void FileTransferTask::onGo()
{
	if ( m_action == Receive )
	{
		//we have to send a signal because liboscar isn't supposed to know about OscarContact.
		Kopete::TransferManager *tm = 0;
		emit getTransferManager( &tm );
		connect( tm, SIGNAL( refused( const Kopete::FileTransferInfo& ) ), this, SLOT( doCancel( const Kopete::FileTransferInfo& ) ) );
		connect( tm, SIGNAL( accepted(Kopete::Transfer*, const QString &) ), this, SLOT( doAccept( Kopete::Transfer*, const QString & ) ) );

		emit askIncoming( m_contactName, m_name, m_size, QString::null, m_cookie );
		//TODO: support icq descriptions
		return;
	} 
	//else, send
	if ( m_contactName.isEmpty() || (! validFile() ) )
	{
		setSuccess( 0 );
		return;
	}

	if ( client()->settings()->fileProxy() )
	{ //proxy stage 1
		m_proxy = 1;
		m_proxyRequester = 1;
		doConnect();
	}
	else
		sendReq();
}

void FileTransferTask::parseReq( Buffer b )
{
	QByteArray proxy_ip;
	//DWORD client_ip = 0;
	QByteArray verified_ip;
	while( b.bytesAvailable() )
	{
		TLV tlv = b.getTLV();
		Buffer b2( tlv.data );
		switch( tlv.type )
		{
		 case 0x2711: //file-specific stuff
			if ( m_action == Send ) //then we don't care
				break;
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "multiple file flag: " << b2.getWord() << " file count: " << b2.getWord() << endl;
			m_size = b2.getDWord();
			m_name = b2.getBlock( b2.bytesAvailable() );
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "size: " << m_size << " file: " << m_name << endl;
			break;
		 case 2:
		 	proxy_ip = tlv.data;
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "proxy ip " << proxy_ip << endl;
			break;
		 case 3:
		 	//client_ip = b2.getDWord();
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "client ip " << tlv.data << endl;
			break;
		 case 4:
		 	verified_ip = tlv.data;
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "verified ip " << verified_ip << endl;
			break;
		 case 5:
		 	m_port = b2.getWord();
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "port " << m_port << endl;
			break;
		 case 0x10:
		 	m_proxy = true;
			break;
		 default:
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "ignoring tlv type " << tlv.type << endl;
		}
	}

	if( m_proxy )
	{
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "proxy requested" << endl;
		m_ip = proxy_ip;
	}
	else
	{
		//for now, only try the verified ip. FIXME
		m_ip = verified_ip;
	}

}

bool FileTransferTask::validFile()
{
	if ( m_action == Send )
	{
		if ( ! m_file.exists() )
		{
			emit error( KIO::ERR_DOES_NOT_EXIST, m_file.fileName() );
			return 0;
		}
		if ( m_file.size() == 0 )
		{
			emit error( KIO::ERR_COULD_NOT_READ, i18n("file is empty: ") + m_file.fileName() );
			return 0;
		}
		if ( ! m_file.open( QIODevice::ReadOnly ) )
		{
			emit error( KIO::ERR_CANNOT_OPEN_FOR_READING, m_file.fileName() );
			return 0;
		}
	}
	else //receive
	{
		if ( ! m_file.open( QIODevice::WriteOnly ) )
		{
			emit error( KIO::ERR_CANNOT_OPEN_FOR_WRITING, m_file.fileName() );
			return 0;
		}
	}
	m_file.close();
	return true;
}

bool FileTransferTask::take( Transfer* transfer )
{
	Q_UNUSED(transfer);
	return false;
}

bool FileTransferTask::take( int type, QByteArray cookie, Buffer b )
{
	kDebug(14151) << k_funcinfo << "comparing to " << m_cookie << endl;
	if ( cookie != m_cookie )
		return false;

	//ooh, ooh, something happened!
	switch( type )
	{
	 case 0: //direct transfer ain't good enough
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "redirect or proxy request" << endl;
		delete m_ss;
		m_ss = 0;
		parseReq( b );
		doConnect();
		break;
	 case 1:
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "other user cancelled filetransfer :(" << endl;
		emit gotCancel();
		setSuccess( true );
		break;
	 case 2:
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "other user acceptetd filetransfer :)" << endl;
		emit gotAccept();
		break;
	 default:
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "bad request type: " << type << endl;
	}
	return true;
}

void FileTransferTask::readyAccept()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "******************" << endl;
	m_connection = dynamic_cast<KBufferedSocket*>( m_ss->accept() );
	m_ss->close(); //free up the port so others can listen
	m_ss->deleteLater();
	m_ss = 0;
	if (! m_connection )
	{ //either it wasn't buffered, or it did something weird
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "connection failed somehow." << endl;
		emit error( KIO::ERR_COULD_NOT_ACCEPT, QString::null );
		doCancel();
		return;
	}
	//ok, so we have a direct connection. for filetransfers. cool.
	//might be a good idea to hook up some signals&slots.
	connect( m_connection, SIGNAL( readyRead() ), this, SLOT( socketRead() ) );
	connect( m_connection, SIGNAL( gotError( int ) ), this, SLOT( socketError( int ) ) );
	//now we can finally send the first OFT packet.
	if ( m_action == Send )
		oftPrompt();
}

void FileTransferTask::socketError( int e )
{ //FIXME: handle this properly for all cases
	QString desc;
	if ( m_ss )
		desc = m_ss->errorString();
	else if ( m_connection )
		desc = m_connection->errorString();
	kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "socket error: " << e << " : " << desc << endl;
	if ( m_state == Connecting )
	{ //connection failed, try another way
		if ( m_proxy )
		{ //fuck, we failed at a proxy! just give up
			emit error( KIO::ERR_COULD_NOT_CONNECT, desc );
			doCancel();
		}
		else
		{
			m_timer.stop();
			connectFailed();
		}
	}
}

void FileTransferTask::socketRead()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;

	switch( m_state )
	{
		case Receiving: //raw file data
			saveData();
			break;
		case ProxySetup: //proxy command
			proxyRead();
			break;
		default: //oft packet
			oftRead();
	}
}

void FileTransferTask::proxyRead()
{
	QByteArray raw = m_connection->readAll(); //is this safe?
	Buffer b( raw );
	WORD length = b.getWord();
	if ( b.bytesAvailable() != length )
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "length is " << length << " but we have " << b.bytesAvailable() << "bytes!" << endl;
	b.skipBytes( 2 ); //protocol version
	WORD command = b.getWord();
	b.skipBytes( 6 ); //4 unknown, 2 flags

	switch( command )
	{
		case 1: //error
		{
			WORD err = b.getWord();
			QString errMsg;
			switch( err )
			{
				case 0x0d: //we did something wrong (eg. wrong username)
				case 0x0e: //we did something wronger (eg. no cookie)
					errMsg = i18n("Bad Request");
					break;
				case 0x10: //we did something else wrong
					errMsg = i18n("Request Timed Out");
					break;
				case 0x1a: //other side was too slow
					errMsg = i18n("Accept Period Timed Out");
					break;
				default:
					errMsg = i18n("Unknown Error: ") + QString::number( err );
			}

			emit error( KIO::ERR_COULD_NOT_LOGIN, errMsg );
			doCancel();
			break;
		}
		case 3: //ack
			m_port = b.getWord();
			m_ip = b.getBlock( 4 );
			//now we send a proxy request to the other side
			sendReq();
			break;
		case 5: //ready
			doneConnect();
	}
}

void FileTransferTask::oftRead()
{
	QByteArray raw = m_connection->readAll(); //is this safe?
	OftProtocol p;
	uint b=0;
	//remember we're responsible for freeing this!
	OftTransfer *t = static_cast<OftTransfer*>( p.parse( raw, b ) );
	OFT data = t->data();
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "checksum: " << data.checksum << endl;
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "sentChecksum: " << data.sentChecksum << endl;
	switch( data.type )
	{
	 case 0x101:
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "prompt" << endl;
		//do we care about anything *in* the prompt?
		//just the checksum.
		m_checksum = data.checksum;
		m_file.open( QIODevice::WriteOnly );
		//TODO what if open failed?
		oftAck();
		break;
	 case 0x202:
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "ack" << endl;
		//time to send real data
		//TODO: validate file again, just to be sure
		m_file.open( QIODevice::ReadOnly );
		//switch the timer over to the other function
		m_timer.disconnect();
		connect( &m_timer, SIGNAL( timeout() ), this, SLOT( write() ) );
		m_timer.start(0);
		break;
	 case 0x204:
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "done" << endl;
		emit fileComplete();
		m_timer.stop();
		if ( data.sentChecksum != checksum() )
			kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "checksums do not match!" << endl;
		setSuccess( true );
		break;
	 case 0x205: //not supported yet
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "receiver resume" << endl;
		doCancel();
		break;
	 case 0x106: //can't happen
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "sender resume" << endl;
		break;
	 case 0x207: //can't happen
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "resume ack" << endl;
		break;
	 default:
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "unknown type " << data.type << endl;
	}

	delete t;
}

void FileTransferTask::write()
{
	if ( m_connection->bytesToWrite() )
		return; //give hte socket time to catch up
	//an arbitrary amount to send each time.
	int max = 256;
	char data[256];
	int read = m_file.read( data, max );
	if( read == -1 )
	{ //FIXME: handle this properly
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "failed to read :(" << endl;
		return;
	}

	int written = m_connection->write( data, read );
	if( written == -1 )
	{ //FIXME: handle this properly
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "failed to write :(" << endl;
		return;
	}

	m_bytes += written;
	if ( written != read ) //FIXME: handle this properly
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "didn't write everything we read" << endl;
	//tell the ui
	emit processed( m_bytes );
	if ( m_bytes >= m_size )
	{
		m_file.close();
		//switch the timer over to the other function
		//we should always get OFT Done before this times out
		//or we could just finish now without waiting
		//but I want to do it this way for now
		m_timer.disconnect();
		connect( &m_timer, SIGNAL( timeout() ), this, SLOT( timeout() ) );
		m_timer.start( 10 * 1000 );
	}
}

void FileTransferTask::saveData()
{
	QByteArray raw = m_connection->readAll(); //is this safe?
	int written = m_file.write( raw );
	if( written == -1 )
	{ //FIXME: handle this properly
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "failed to write :(" << endl;
		return;
	}
	m_bytes += written;
	if ( written != raw.size() ) //FIXME: handle this properly
		kWarning(OSCAR_RAW_DEBUG) << k_funcinfo << "didn't write everything we read" << endl;
	//tell the ui
	emit processed( m_bytes );
	if ( m_bytes >= m_size )
	{
		m_file.close();
		oftDone();
		emit fileComplete();
		setSuccess( true );
	}

}

OFT FileTransferTask::makeOft()
{
	//fill an OFT with data
	OFT data;
	data.type = 0; //invalid
	data.cookie = m_cookie;
	data.fileSize = m_size;
	data.modTime = QFileInfo( m_file ).lastModified().toTime_t();
	data.checksum = 0xFFFF0000; //file checksum
	data.bytesSent = 0;
	data.sentChecksum = 0xFFFF0000; //checksum of transmitted bytes
	data.flags = 0x20; //flags; 0x20=not done, 1=done
	data.fileName = m_name;
	return data;
}

void FileTransferTask::sendOft( OFT data )
{
	//now make a transfer out of it
	OftTransfer t( data );
	int written = m_connection->write( t.toWire() );

	if( written == -1 ) //FIXME: handle this properly
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "failed to write :(" << endl;
}

void FileTransferTask::oftPrompt()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	OFT data = makeOft();
	data.type = 0x0101; //type = prompt
	data.checksum = checksum();
	sendOft( data );
	//now we wait for the other side to ack
}

void FileTransferTask::oftAck()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	OFT data = makeOft();
	data.type = 0x0202; //type = ack
	sendOft( data );
	m_state = Receiving;
}

void FileTransferTask::oftDone()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	OFT data = makeOft();
	data.type = 0x0204; //type = done
	data.sentChecksum = checksum();
	if ( data.sentChecksum != m_checksum )
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "checksums do not match!" << endl;
	data.flags = 1;
	sendOft( data );
}

void FileTransferTask::doCancel()
{
	Oscar::Message msg = makeFTMsg();
	msg.setReqType( 1 );

	emit sendMessage( msg );
	setSuccess( true );
}


void FileTransferTask::doCancel( const Kopete::FileTransferInfo &info )
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	//check that it's really for us
	if ( info.internalId() == QString( m_cookie ) )
		doCancel();
	else
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "ID mismatch" << endl;
}

void FileTransferTask::doAccept( Kopete::Transfer *t, const QString & localName )
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	//check that it's really for us
	//XXX because qt is retarded, I can't simply compare a qstring and bytearray any more.
	//if there are 0's in hte cookie then the qstring will only contain part of it - but it should at least be consistent about that. it just slightly increases the tiny chance of a conflict
	if ( t->info().internalId() != QString( m_cookie ) )
	{
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "ID mismatch" << endl;
		return;
	}

	//TODO: we should unhook the old transfermanager signals now

	//hook up the ones for the transfer
	connect( this , SIGNAL( gotCancel() ), t, SLOT( slotCancelled() ) );
	connect( this , SIGNAL( error( int, const QString & ) ), t, SLOT( slotError( int, const QString & ) ) );
	connect( this , SIGNAL( processed( unsigned int ) ), t, SLOT( slotProcessed( unsigned int ) ) );
	connect( this , SIGNAL( fileComplete() ), t, SLOT( slotComplete() ) );
	//and save the chosen filename
	m_file.setFileName( localName );

	doConnect();
}

void FileTransferTask::doConnect()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	if( ! validFile() )
	{
		doCancel();
		return;
	}

	QString host;
	if ( m_proxyRequester )
		host = "ars.oscar.aol.com";
	else
	{
		if ( m_ip.length() != 4 || ! m_port )
		{
			emit error( KIO::ERR_COULD_NOT_CONNECT, i18n("missing ip or port") ); 
			doCancel();
			return;
		}

		//ksockets demand a qstring
		host = KNetwork::KIpAddress( m_ip.constData(), 4 ).toString();
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "ip: " << host << endl;
	}

	//proxies *always* use port 5190; the "port" value is some retarded check
	m_connection = new KBufferedSocket( host, QString::number( m_proxy ? 5190 : m_port ) );
	connect( m_connection, SIGNAL( readyRead() ), this, SLOT( socketRead() ) );
	connect( m_connection, SIGNAL( gotError( int ) ), this, SLOT( socketError( int ) ) );
	connect( m_connection, SIGNAL( connected(const KNetwork::KResolverEntry&)), this, SLOT(socketConnected()));

	m_state = Connecting;
	//socket doesn't seem to have its own timeout, so here's mine
	connect( &m_timer, SIGNAL( timeout() ), this, SLOT( timeout() ) );
	m_timer.start( 10 * 1000 ); //TODO: maybe let the user set timeout
	//try it
	m_connection->connect();
}

void FileTransferTask::socketConnected()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	m_timer.stop();
	if( m_proxy )
	//we need to send an init recv first
		proxyInit();
	else
		doneConnect();
}

void FileTransferTask::doneConnect()
{
	m_state = Default;
	//TODO: proxy requester doesn't send accept
	//yay! send an accept message
	Oscar::Message msg = makeFTMsg();
	msg.setReqType( 2 );
	emit sendMessage( msg );
	//next the receiver should get a prompt from the sender.
	if ( m_action == Send )
		oftPrompt();
}

void FileTransferTask::proxyInit()
{
	m_state = ProxySetup;
	//init "send" is sent by whoever requested the proxy.
	//init "recv" is sent by the other side
	//because the second person has to include the port check
	
	Buffer data;
	data.addByte( m_selfName.length() );
	data.addString( m_selfName.toLatin1() );
	if (! m_proxyRequester ) //if 'recv'
		data.addWord( m_port );
	data.addString( m_cookie );
	//cap tlv
	data.addDWord( 0x00010010 );
	data.addGuid( oscar_caps[ CAP_SENDFILE ] );

	Buffer header;
	header.addWord( 10 + data.length() ); //length
	header.addWord( 0x044a ); //packet version
	header.addWord( m_proxyRequester ? 2 : 4 ); //command: 2='send' 4='recv'
	header.addDWord( 0 ); //unknown
	header.addWord( 0 ); //flags

	header.addString( data );
	//send it to the proxy server
	int written = m_connection->write( header.buffer() );

	if( written == -1 ) //FIXME: handle this properly
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "failed to write :(" << endl;
	
}

void FileTransferTask::timeout()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	m_timer.stop();
	if ( m_state == Connecting )
	{ //kbufferedsocket took too damn long
		if ( m_proxy )
		{ //fuck, we failed at a proxy! just give up
			emit error( KIO::ERR_COULD_NOT_CONNECT, i18n("Timeout") );
			doCancel();
		}
		else
			connectFailed();
		return;
	}

	//nothing's happened for ages - assume we're dead.
	//so tell the user, send off a cancel, and die
	emit error( KIO::ERR_ABORTED, i18n("Timeout") );
	doCancel();
}

void FileTransferTask::connectFailed()
{
	delete m_connection;
	m_connection = 0;
	bool proxy = client()->settings()->fileProxy();
	if ( m_action == Receive && (! proxy ) )
	{ //try redirect
		m_state = Default;
		sendReq();
	}
	else
	{ //proxy stage 2 or 3
		m_proxy = 1;
		m_proxyRequester = 1;
		doConnect();
	}
}

bool FileTransferTask::listen()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	//listen for connections
	m_ss = new KServerSocket( this );
	connect( m_ss, SIGNAL(readyAccept()), this, SLOT(readyAccept()) );
	connect( m_ss, SIGNAL(gotError(int)), this, SLOT(socketError(int)) );
	bool success = false;
	int first = client()->settings()->firstPort();
	int last = client()->settings()->lastPort();
	//I don't trust the settings to be sane
	if ( last < first )
		last = first;
	for ( m_port = first; m_port <= last; ++m_port )
	{ //try ports in the range (default 5190-5199)
		m_ss->setAddress( QString::number( m_port ) );
		if( success = ( m_ss->listen() && m_ss->error() == KNetwork::KSocketBase::NoError ) )
			break;
		m_ss->close();
	}
	if (! success )
	{ //uhoh... what do we do? FIXME: maybe tell the user too many filetransfers
		kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "listening failed. abandoning" << endl;
		emit error( KIO::ERR_COULD_NOT_LISTEN, QString::number( m_port ) );
		setSuccess(false);
		return false;
	}
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << "listening for connections on port " << m_port << endl;
	return true;

}

void FileTransferTask::sendReq()
{
	//if we're not using a proxy we need a working serversocket
	if (!( m_proxy || listen() ))
		return;

	Buffer b;
	b.addString( m_cookie );

	//set up a message for sendmessagetask
	Oscar::Message msg = makeFTMsg();

	//now set the rendezvous info
	msg.setReqType( 0 );
	msg.setPort( m_port );
	msg.setFile( m_size, m_name );
	if ( m_proxy )
		msg.setProxy( m_ip );

	if ( m_action == Receive )
		msg.setReqNum( 2 );
	//TODO: could be 3

	//we're done, send it off!
	emit sendMessage( msg );
}

Oscar::Message FileTransferTask::makeFTMsg()
{
	Oscar::Message msg;
	msg.setMessageType( 3 ); //filetransfer
	msg.setChannel( 2 ); //rendezvous
	msg.setIcbmCookie( m_cookie );
	msg.setReceiver( m_contactName );
	return msg;
}

DWORD FileTransferTask::checksum()
{
	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << endl;
	//code adapted from joscar's FileTransferChecksum
	DWORD check = 0x0000ffff;
	m_file.open( QIODevice::ReadOnly );

	char b;
	while( m_file.getChar( &b ) ) {
		DWORD oldcheck = check;

		int val = ( b & 0xff ) << 8;
		if ( m_file.getChar( &b ) )
			val += ( b & 0xff );

		check -= val;

		if (check > oldcheck)
			check--;
	}
	m_file.close();

	check = ((check & 0x0000ffff) + (check >> 16));
	check = ((check & 0x0000ffff) + (check >> 16));
	check = check << 16;

	kDebug(OSCAR_RAW_DEBUG) << k_funcinfo << check << endl;
	return check;
}

#include "filetransfertask.moc"


