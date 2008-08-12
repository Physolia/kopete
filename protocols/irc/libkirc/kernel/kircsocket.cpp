/*
    kircsocket.cpp - IRC socket.

    Copyright (c) 2002      by Nick Betcher <nbetcher@kde.org>
    Copyright (c) 2003-2007 by Michel Hermier <michel.hermier@gmail.com>
    Copyright (c) 2006      by Tommi Rantala <tommi.rantala@cs.helsinki.fi>

    Kopete    (c) 2002-2007 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "kircsocket.moc"
#include "kircsocket_p.moc"

#include "kirccontext.h"

#include <qtextcodec.h>
#include <qtimer.h>

using namespace KIrc;

SocketPrivate::SocketPrivate(KIrc::Socket *socket)
		: q_ptr(socket)
		, socket(0)
		, state(KIrc::Socket::Idle)
{
}

void SocketPrivate::setSocket(QAbstractSocket *socket)
{
	connect(socket, SIGNAL(readyRead()), SLOT(onReadyRead()));
	connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
		SLOT(socketStateChanged(QAbstractSocket::SocketState)));
	connect(socket, SIGNAL(error(QAbstractSocket::SocketError)),
		SLOT(socketGotError(QAbstractSocket::SocketError)));
}

void SocketPrivate::setConnectionState(Socket::ConnectionState newstate)
{
	Q_Q(Socket);

	if (state != newstate)
	{
		state = newstate;
		emit q->connectionStateChanged(newstate);
	}
}


void SocketPrivate::socketGotError(QAbstractSocket::SocketError)
{
	/*
	KBufferedSocket::SocketError err = d->socket->error();

	// Ignore spurious error
	if (err == KBufferedSocket::NoError)
		return;

	QString errStr = d->socket->errorString();
	kDebug(14121) << "Socket error: " << errStr;
//	postErrorEvent(errStr);

	// ignore non-fatal error
	if (!KBufferedSocket::isFatalError(err))
		return;

	close();
	*/
}

void SocketPrivate::socketReadyRead()
{
	Q_Q(Socket);

	// The caller can also be self so lets check the socket still exist
	
	if (socket && socket->canReadLine())
	{
		bool ok = false;
		QByteArray raw = socket->readLine();

		Message msg = Message::fromLine(raw, &ok);
//		msg.setDirection(Message::InGoing);

		if (ok)
			emit q->receivedMessage(msg);
//		else
//			postErrorEvent(i18n("Parse error while parsing: %1").arg(msg.rawLine()));

		// FIXME: post events instead of reschudeling.
		QTimer::singleShot( 0, this, SLOT( onReadyRead() ) );
	}

//	if(d->socket->socketStatus() != KExtendedSocket::connected)
//		error();
}

void SocketPrivate::socketStateChanged(QAbstractSocket::SocketState newstate)
{
	Q_Q(Socket);

	switch (newstate) {
	case QAbstractSocket::UnconnectedState:
		setConnectionState(Socket::Idle);
		break;
	case QAbstractSocket::HostLookupState:
		setConnectionState(Socket::HostLookup);
		break;
	case QAbstractSocket::ConnectingState:
		setConnectionState(Socket::Connecting);
		break;
//	MUST BE HANDLED BY CHILDREN
//	case QAbstractSocket::ConnectedState:
//		setConnectionState(Socket::Open);
//		break;
	case QAbstractSocket::ClosingState:
		setConnectionState(Socket::Closing);
		break;
	default:
//		postErrorEvent(i18n("Unknown SocketState value:%1", newstate));
		q->close();
	}
}

Socket::Socket(Context *context, SocketPrivate *socketp)
	: QObject(context)
	, d_ptr(socketp)
{
	Q_D(Socket);

	d->owner = new Entity(context);
}

Socket::~Socket()
{
	delete d_ptr;
}

Socket::ConnectionState Socket::connectionState() const
{
	Q_D(const Socket);

	return d->state;
}

QAbstractSocket *Socket::socket()
{
	Q_D(Socket);

	return d->socket;
}

Entity::Ptr Socket::owner() const
{
	Q_D(const Socket);

	return d->owner;
}

void Socket::writeMessage(const Message &msg)
{
	Q_D(Socket);

#ifdef __GNUC__
	#warning Check message validity before sending it
#endif

//	if (!d->socket || d->socket->state() != QAbstractSocket::ConnectedState)
	{
//		postErrorEvent(i18n("Attempting to send while not connected: %1", msg.data()));
		return;
	}

//	qint64 wrote = d->socket->write(msg.toLine());

//	if (wrote == -1)
//		kDebug(14121) << "Socket write failed!";

//	kDebug(14121) << QString::fromLatin1("(%1 bytes) >> %2").arg(wrote).arg(rawMsg);
}

void Socket::close()
{
	Q_D(Socket);

	delete d->socket;
	d->socket = 0;

	d->url = "";
}

#if 0
void Socket::showInfoDialog()
{
	if( d->useSSL )
	{
		static_cast<KSSLSocket*>( d->socket )->showInfoDialog();
	}
}

QByteArray Socket::encode(const QString &str, bool *success, QTextCodec *codec) const
{
	kDebug(14121) ;
	*success = false;

	if (!codec || !codec->canEncode(str))
	{
		if (!d->defaultCodec || !d->defaultCodec->canEncode(str))
		{
			if (!UTF8 || !UTF8->canEncode(str))
			{
//				postErrorEvent(i18n("Codec Error: .").arg(codec->name()));
				return QByteArray();
			}
			else
			{
//				postErrorEvent(i18n("Codec Warning: Using codec %1 for:\n%2.")
//					.arg(UTF8->name(), str));
				codec = UTF8;
			}
		}
		else
			codec = d->defaultCodec;
	}

	*success = true;
	return codec->fromUnicode(str);
}
#endif
