//
// C++ Implementation: eventprotocol
//
// Description: 
//
//
// Author: SUSE AG <>, (C) 2004
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "gwerror.h"

#include "eventtransfer.h"
#include "eventprotocol.h"

using namespace GroupWise;

EventProtocol::EventProtocol(QObject *parent, const char *name)
 : InputProtocolBase(parent, name)
{
}

EventProtocol::~EventProtocol()
{
}

Transfer * EventProtocol::parse( const QByteArray & wire, uint& bytes )
{
	m_bytes = 0;
	m_din = new QDataStream( wire, IO_ReadOnly );
	m_din->setByteOrder( QDataStream::LittleEndian );
	Q_UINT32 type;

	if ( !okToProceed() )
		return 0;
	// read the event type
	*m_din >> type;
	m_bytes += sizeof( Q_UINT32 );
	
	qDebug( "EventProtocol::parse() Reading event of type %i", type );
	if ( type > Stop )
	{
		qDebug( "EventProtocol::parse() - found unexpected event type %i - assuming out of sync", type );
		m_state = OutOfSync;
		return 0;
	}

	// read the event source
	QString source;
	if ( !readString( source ) )
		return 0;
	
	// now create an event object
	//HACK: lowercased DN
	EventTransfer * tentative = new EventTransfer( type, source.lower(), QDateTime::currentDateTime() );

	// add any additional data depending on the type of event
	// Note: if there are any errors in the way the data is read below, we will soon be OutOfSync
	QString statusText;
	QString guid;
	Q_UINT16 status;
	Q_UINT32 flags;
	QString message;
	
	switch ( type )
	{
		case StatusChange: //103 - STATUS + STATUSTEXT
			if ( !okToProceed() )
				return 0;
			*m_din >> status;
			m_bytes += sizeof( Q_UINT16 );
			if ( !readString( statusText ) )
				return 0;
			qDebug( "got status: %i", status );
			tentative->setStatus( status );
			qDebug( "tentative status: %i", tentative->status() );
			tentative->setStatusText( statusText );
			break;
		case ConferenceJoined:		// 106 - GUID + FLAGS
		case ConferenceLeft:		// 107
			if ( !readString( guid ) )
				return 0;
			tentative->setGuid( guid );
			if ( !readFlags( flags ) )
				return 0;
			tentative->setFlags( flags );
			break;
		case UndeliverableStatus:	//102 - GUID
		case ConferenceClosed:		//105
		case ConferenceInviteNotify://118
		case ConferenceReject:		//119
		case UserTyping:			//112
		case UserNotTyping:			//113
			if ( !readString( guid ) )
				return 0;
			tentative->setGuid( guid );
			break;
		case ReceiveAutoReply:		//121 - GUID + FLAGS + MESSAGE
		case ReceiveMessage:		//108
			// guid
			if ( !readString( guid ) )
				return 0;
			tentative->setGuid( guid );
			// flags
			if ( !readFlags( flags ) )
				return 0;
			tentative->setFlags( flags );
			// message
			if ( !readString( message ) )
				return 0;
			tentative->setMessage( message );
			break;
		case ConferenceInvite:		//117 GUID + MESSAGE
			// guid
			if ( !readString( guid ) )
				return 0;
			tentative->setGuid( guid );
			// message
			if ( !readString( message ) )
				return 0;
			tentative->setMessage( message );
			break;
		case UserDisconnect:		//114 (NOTHING)
		case ServerDisconnect:		//115
			// nothing else to read
			break;
		case InvalidRecipient:		//101
		case ContactAdd:			//104
		case ReceiveFile:			//109
		case ConferenceRename:		//116
			// unhandled because unhandled in Gaim
			break;
		default:
			qDebug( "EventProtocol::parse() - found unexpected event type %i", type );
			break;
	}
	// if we got this far, the parse succeeded, return the Transfer
	m_state = Success;
	delete m_din;
	bytes = m_bytes;
	return tentative;
}

bool EventProtocol::readFlags( Q_UINT32 &flags)
{
	if ( okToProceed() )
	{
		*m_din >> flags;
		m_bytes += sizeof( Q_UINT32 );
		return true;
	}
	return false;
}


#include "eventprotocol.moc"
