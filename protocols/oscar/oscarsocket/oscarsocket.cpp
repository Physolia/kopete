/*
  oscarsocket.cpp  -  Oscar Protocol Implementation

    Copyright (c) 2002 by Tom Linsky <twl6@po.cwru.edu>
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

#include "config.h"

#include "oscarsocket.h"
#include "rateclass.h"
#include "oscardebug.h"

#include <stdlib.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <netinet/in.h> // for htonl()

#include "oscaraccount.h"

#include <qobject.h>
#include <qtextcodec.h>
#include <qtimer.h>

#include <kdebug.h>

// ---------------------------------------------------------------------------------------

OscarSocket::OscarSocket(const QString &connName, const QByteArray &cookie,
	OscarAccount *account, QObject *parent, const char *name, bool isicq)
	: OscarConnection(connName, Server, cookie, parent, name)
{
//	kdDebug(14151) << k_funcinfo << "connName=" << connName <<
//		QString::fromLatin1( isicq?" ICQ":" AIM" ) << endl;

	mIsICQ=isicq;
	toicqsrv_seq=0;
	type2SequenceNum=0xFFFF;
	flapSequenceNum=rand() & 0x7FFF; // value taken from libicq
	mPwEncryptionKey=0L;
	mCookie=0L;
	loginStatus=0;
	gotAllRights=0;
	keepaliveTime=30;
	keepaliveTimer=0L;
	rateClasses.setAutoDelete(TRUE);
	isLoggedIn=false;
	idle=false;
	//mDirectConnnectionCookie=rand();
	mAccount=account;
	//mDirectIMMgr=0L;
	//mFileTransferMgr=0L;
	//bSomethingOutgoing=false;

	connect(this, SIGNAL(socketClosed(const QString &, bool)),
		this, SLOT(slotConnectionClosed(const QString &, bool)));
	connect(this, SIGNAL(moreToRead()),
		this, SLOT(slotRead()));
	/*
	connect(this, SIGNAL(serverReady()), this, SLOT(OnServerReady()));
	*/
}

OscarSocket::~OscarSocket()
{
	kdDebug(14151) << k_funcinfo << "Called" << endl;

	if(socketStatus() == OscarConnection::Connecting ||
		socketStatus() == OscarConnection::Connected )
	{
		kdDebug(14151) << k_funcinfo <<
			"ERROR: WE ARE NOT DISCONNECTED YET" << endl;

		stopKeepalive();

		disconnect(mSocket, 0, 0, 0);
		mSocket->reset();
	}

	//delete mDirectIMMgr;
	//delete mFileTransferMgr;
	delete[] mCookie;
	delete[] mPwEncryptionKey;

	for (RateClass *tmp=rateClasses.first(); tmp; tmp=rateClasses.next())
		disconnect(tmp, SIGNAL(dataReady(Buffer &)), this, SLOT(writeData(Buffer &)));
	rateClasses.clear();
}

DWORD OscarSocket::setIPv4Address(const QString &address)
{
	kdDebug(14151) << "Setting IP address to: " << address << endl;
	QString a=address.simplifyWhiteSpace();

	QStringList ipv4Addr=QStringList::split(".", a, FALSE);
	if (ipv4Addr.count() == 4)
	{
		unsigned long newAddr=0;
		int i=0;
		bool ok=true;
		while (ok && i < 4)
		{
			unsigned long value=ipv4Addr[i].toUInt(&ok);
			if (value > 255)
				ok=false;
			if (ok)
				newAddr=newAddr * 256 + value;
			i++;
		}
		if (ok)
			return newAddr;
	}
	return 0;
}

SSIData& OscarSocket::ssiData()
{
	return mSSIData;
}

void OscarSocket::slotConnected()
{
	kdDebug(14151) << k_funcinfo <<
		"Connected to '" << peerHost() <<
		"', port '" << peerPort() << "'" << endl;

#if 0
	QString h=mSocket->localAddress()->nodeName();
	mDirectIMMgr=new OncomingSocket(this, h, DirectIM);
	mFileTransferMgr=new OncomingSocket(this, h, SendFile, SENDFILE_PORT);
#endif
	//emit connectionChanged(1, QString("Connected to %2, port %1").arg(peerPort()).arg(peerName()));
}

void OscarSocket::slotConnectionClosed(const QString & /*connName */, bool expected)
{
	kdDebug(14151) << k_funcinfo << "Connection for account '" <<
		mAccount->accountId() << "' closed, expected = " << expected << endl;

	if(mSocket->bytesAvailable() > 0)
	{
		kdDebug(14151) << k_funcinfo <<
			mSocket->bytesAvailable() <<
			" bytes were left to read, maybe connection broke down?" << endl;
	}

	stopKeepalive();
	rateClasses.clear();
	loginStatus=0;
	idle=false;
	gotAllRights=0;
	isLoggedIn=false;

	disconnect(this, SIGNAL(connAckReceived()), 0, 0);
	disconnect(this, SIGNAL(socketConnected(const QString &)), 0, 0);

	mSocket->reset();

	/*if(mDirectIMMgr)
	{
		delete mDirectIMMgr;
		mDirectIMMgr=0L;
	}

	if(mFileTransferMgr)
	{
		delete mFileTransferMgr;
		mFileTransferMgr=0L;
	}*/

	//kdDebug(14151) << k_funcinfo << "emitting statusChanged(OSCAR_OFFLINE)" << endl;
	emit statusChanged(OSCAR_OFFLINE);

	if (!expected)
		mAccount->disconnect(Kopete::Account::ConnectionReset);
}


void OscarSocket::slotRead()
{
	//kdDebug(14151) << k_funcinfo << "-----------------------" << endl;

	//int waitCount=0;
	char *buf=0L;
	Buffer inbuf;
	FLAP fl;
	int bytesread=0;

	/*kdDebug(14151) << k_funcinfo <<
		mSocket->bytesAvailable() << " bytes to read" << endl;*/

	// a FLAP is min 6 bytes, we cannot read more out of
	// a buffer without these 6 initial bytes

	fl = getFLAP();

	if (fl.error || fl.length == 0)
	{
		kdDebug(14151) << k_funcinfo <<
			"Could not read full FLAP, aborting" << endl;
		return;
	}

	buf = new char[fl.length];

	bytesread = mSocket->readBlock(buf, fl.length);
	if(bytesread != fl.length)
	{
		kdDebug(14151) << k_funcinfo << "WARNING, couldn't read all of" \
			" that packet, this will probably break things!!!" << endl;
	}

	inbuf.setBuf(buf, bytesread);

#ifdef OSCAR_PACKETLOG
	kdDebug(14151) << "=== INPUT ===" << inbuf.toString();
#endif

	//kdDebug(14151) << "FLAP channel is " << fl.channel << endl;

	switch(fl.channel)
	{
		case 0x01: //new connection negotiation channel
		{
			DWORD flapversion=inbuf.getDWord();
			if (flapversion == 0x00000001)
			{
				emit connAckReceived();
			}
			else
			{
				kdDebug(14151) << k_funcinfo <<
					"!Could not read FLAP version on channel 0x01" << endl;
				break;
			}
			break;
		} // END 0x01

		case 0x02: //snac data channel
		{
			SNAC s;
			s=inbuf.getSnacHeader();

			if (s.family != 3 && s.subtype != 11) // avoid the spam of all the online notifies
			{
				kdDebug(14151) << k_funcinfo << "SNAC(" << s.family << "," <<
					s.subtype << "), id=" << s.id << endl;
			}

			switch(s.family)
			{
				case OSCAR_FAM_1: // Service Controls
				{
					switch(s.subtype)
					{
						case 0x0001:
							kdDebug(14151) << k_funcinfo << "SNAC Family 1 error" << endl;
							parseError(s.family, s.id, inbuf);
							break;
						case 0x0003: //server ready
							parseServerReady(inbuf);
							break;
						case 0x0005: //redirect
							kdDebug(14151) << k_funcinfo <<
								"TODO: Parse redirect! ================" << endl;
							//parseRedirect(inbuf);
							break;
						case 0x0007: //rate info request response, SRV_RATES
							parseRateInfoResponse(inbuf);
							break;
						case 0x000f: //my user info
							parseMyUserInfo(inbuf);
							break;
						case 0x000a: //rate change
							parseRateChange(inbuf);
							break;
						case 0x0010: //warning notification
							parseWarningNotify(inbuf);
							break;
						case 0x0013: //message of the day
							parseMessageOfTheDay(inbuf);
							break;
						case 0x0018: //server versions
							parseServerVersions(inbuf);
							break;
						case 0x001f: //requests a memory segment, part of aim.exe  EVIL AOL!!!
							parseMemRequest(inbuf);
							break;
						default:
							kdDebug(14151) << k_funcinfo << "Unknown SNAC(" <<
								s.family << ",|" << s.subtype << "|)" << endl;
					};
					break;
				} // END OSCAR_FAM_1

				case OSCAR_FAM_2: // Location service
				{
					switch(s.subtype)
					{
						case 0x0001:
							kdDebug(14151) << k_funcinfo << "SNAC Family 2 error" << endl;
							parseError(s.family, s.id, inbuf);
							break;
						case 0x0003: //locate rights
							parseLocateRights(inbuf);
							break;
						case 0x0006: //user info (AIM)
							parseUserLocationInfo(inbuf);
							break;
						default:
							kdDebug(14151) << k_funcinfo << "Unknown SNAC(" <<
								s.family << ",|" << s.subtype << "|)" << endl;
					};
					break;
				} // END 0x0002

				case OSCAR_FAM_3: // Contact services
				{
					switch(s.subtype)
					{
						case 0x0001: // contact list error (SRV_CONTACTERR)
							kdDebug(14151) << k_funcinfo << "SNAC Family 3 error" << endl;
							kdDebug(14151) << k_funcinfo <<
								"RECV SRV_CONTACTERR, UNHANDLED!!!" << endl;
							break;
						case 0x0003: //buddy list rights
							parseBuddyRights(inbuf);
							break;
						case 0x000a: // server refused adding contact to list (SRV_REFUSED)
							kdDebug(14151) << k_funcinfo <<
								"RECV SRV_REFUSED, UNHANDLED!!!" << endl;
							break;
						case 0x000b: //contact changed status, (SRV_USERONLINE)
							parseUserOnline(inbuf);
							break;
						case 0x000c: //contact went offline
							parseUserOffline(inbuf);
							break;
						default:
							kdDebug(14151) << k_funcinfo << "Unknown SNAC(" <<
								s.family << ",|" << s.subtype << "|)" << endl;
					};
					break;
				} // END 0x0003

				case OSCAR_FAM_4: //msg services
				{
					switch(s.subtype)
					{
						case 0x0001: //msg error
							kdDebug(14151) << k_funcinfo << "SNAC Family 4 (ICBM) error" << endl;
							parseError(s.family, s.id, inbuf);
							break;
						case 0x0005: //msg rights SNAC(4,5)
							parseMsgRights(inbuf);
							break;
						case 0x0007: //incoming IM SNAC(4,7)
							parseIM(inbuf);
							break;
						case 0x000a: //missed messages SNAC(4,10)
							parseMissedMessage(inbuf);
							break;
						case 0x000b: // message ack SNAC(4,11)
							parseMsgAck(inbuf);
							break;
						case 0x000c: // server ack for type-2 message SNAC(4,12)
							parseSrvMsgAck(inbuf);
							break;
						case 0x0014: // Mini-Typing notification
							parseMiniTypeNotify(inbuf);
							break;
						default: //invalid subtype
							kdDebug(14151) << k_funcinfo << "Unknown SNAC("
								<< s.family << ",|" << s.subtype << "|)" << endl;
					};
					break;
				} // END OSCAR_FAM_4

				case OSCAR_FAM_9: // BOS
				{
					switch(s.subtype)
					{
						case 0x0001: //BOS error
							kdDebug(14151) << k_funcinfo << "SNAC Family 9 (BOS) error" << endl;
							parseError(s.family, s.id, inbuf);
							break;
						case 0x0003: //bos rights incoming
							parseBOSRights(inbuf);
							break;
						default:
							kdDebug(14151) << k_funcinfo << "Unknown SNAC("
								<< s.family << ",|" << s.subtype << "|)" << endl;
					};
					break;
				} // END OSCAR_FAM_9

				case OSCAR_FAM_19: // Contact list management (SSI)
				{
					switch(s.subtype)
					{
						case 0x0003: //ssi rights
							parseSSIRights(inbuf);
							break;
						case 0x0006: //buddy list
							parseSSIData(inbuf);
							break;
						case 0x000e: //server ack for adding/changing roster items
							parseSSIAck(inbuf, s.id);
							break;
						case 0x000f: //ack when contactlist timestamp/length matches those values sent
							parseSSIOk(inbuf);
							break;
						case 0x001b: // auth reply
							parseAuthReply(inbuf);
							break;
						case 0x001c: // "added by" message
							kdDebug(14151) << k_funcinfo << "IGNORE 'added by' message" << endl;
							break;
						default: //invalid subtype
							kdDebug(14151) << k_funcinfo << "Unknown SNAC("
								<< s.family << ",|" << s.subtype << "|)" << endl;
					};
					break;
				} // END OSCAR_FAM_19

				case OSCAR_FAM_21: // ICQ 0x15 packets
				{
					switch(s.subtype)
					{
						case 0x0001:
							parseError(s.family, s.id, inbuf);
							break;
						case 0x0003:
							parseSRV_FROMICQSRV(inbuf);
							break;
						default:
							kdDebug(14151) << k_funcinfo << "Unknown SNAC("
								<< s.family << ",|" << s.subtype << "|)" << endl;
					}
					break;
				} // END OSCAR_FAM_21

				case OSCAR_FAM_23: //authorization family, TODO: also for icq registration
				{
					switch(s.subtype)
					{
						case 0x0001: //registration refused!
							emit protocolError(
								i18n("Registration refused. Check your user name " \
									"and password and try again"), -1, false);
							break;
						case 0x0003: //authorization response (and hash) is being sent
							parseAuthResponse(inbuf);
							break;
						case 0x0007: //encryption key is being sent
							parsePasswordKey(inbuf);
							break;
						default:
							kdDebug(14151) << k_funcinfo << "Unknown SNAC(" << s.family << ",|" << s.subtype << "|)" << endl;
					};
					break;
				} // END OSCAR_FAM_23

				default:
					kdDebug(14151) << k_funcinfo << "Unknown SNAC(|" << s.family << "|," << s.subtype << ")" << endl;

			}; // END switch (s.family)
			break;
		} // END channel 0x02

		case 0x03: //FLAP error channel
		{
			kdDebug(14151) << "FLAP error channel, UNHANDLED" << endl;
			break;
		} // END channel 0x03

		case 0x04: //close connection negotiation channel
		{
			parseConnectionClosed(inbuf);
			break;
		}

		case 0x05:
		{
			kdDebug(14151) << k_funcinfo << "RECV KEEPALIVE" << endl;
			break;
		}

		default: //oh, crap, something is really wrong
		{
			kdDebug(14151) << k_funcinfo << "Unknown channel " << fl.channel << endl;
		}

	} // END switch(fl.channel)

	inbuf.clear(); // separate buf from inbuf again
	delete [] buf;

	// another flap is waiting in read buffer
	if (mSocket->bytesAvailable() > 0)
		QTimer::singleShot(0, this, SLOT(slotRead()));
}


void OscarSocket::sendLoginRequest()
{
	kdDebug(14151) << k_funcinfo <<
		"SEND CLI_AUTH_REQUEST, sending login request" << endl;

	Buffer outbuf;
	outbuf.addSnac(OSCAR_FAM_23, 0x0006, 0x0000, 0x00000000);
	outbuf.addTLV(0x0001, getSN().length(), getSN().latin1()); // login name
	outbuf.addDWord(0x004B0000); // empty TLV 0x004B
	outbuf.addDWord(0x005A0000); // empty TLV 0x005A

	sendBuf(outbuf, 0x02);
//	emit connectionChanged(2,QString("Requesting login for " + getSN() + "..."));
}


void OscarSocket::putFlapVer(Buffer &outbuf)
{
	outbuf.addDWord(0x00000001);
}


void OscarSocket::OnConnAckReceived()
{
	//kdDebug(14151) << k_funcinfo << "Called." << endl;
	if(mIsICQ)
	{
		//kdDebug(14151) << k_funcinfo << "ICQ-LOGIN, sending ICQ login" << endl;
		sendLoginICQ();
	}
	else
	{
		//kdDebug(14151) << k_funcinfo << "AIM-LOGIN, Sending flap version to server" << endl;
		Buffer outbuf;
		putFlapVer(outbuf);
		sendBuf(outbuf,0x01);

		sendLoginRequest();
	}
}


void OscarSocket::sendBuf(Buffer &outbuf, BYTE chan)
{
	//For now, we use 0 as the sequence number because rate
	//limiting can cause packets from different classes to be
	//sent out in different order
	outbuf.addFlap(chan, 0);

	//Read SNAC family/type from buffer if able
	SNAC s = outbuf.readSnacHeader();

	//if the snac was read without a problem, find its rate class
	if (!s.error)
	{
		//Pointer to proper rate class
		RateClass *rc = 0L;

		//Determine rate class
		for (RateClass *tmp=rateClasses.first(); tmp; tmp=rateClasses.next())
		{
			if (tmp->isMember(s))
			{
				/*
				kdDebug(14151) << k_funcinfo << "Rate class (id=" << tmp->id() <<
					") found: SNAC(" << s.family << "," << s.subtype << ")" << endl;
				*/
				rc = tmp;
				break;
			}
		}

		if (rc)
			rc->enqueue(outbuf);
		else
			writeData(outbuf);
	}
	else
	{
		writeData(outbuf);
	}
}


void OscarSocket::writeData(Buffer &outbuf)
{
	if(socketStatus() != OscarConnection::Connected)
	{
		kdDebug(14151) << k_funcinfo <<
			"Socket is NOT open, can't write to it right now" << endl;
		return;
	}

	//Update packet sequence number
	outbuf.changeSeqNum(flapSequenceNum);
	flapSequenceNum++;

	//kdDebug(14151) << k_funcinfo << "Writing data" << endl;
#ifdef OSCAR_PACKETLOG
	kdDebug(14151) << "--- OUTPUT ---" << outbuf.toString() << endl;
#endif

	//actually put the data onto the wire
	if(mSocket->writeBlock(outbuf.buffer(), outbuf.length()) == -1)
	{
		kdDebug(14151) << k_funcinfo <<
			"Could not write to socket, writeBlock() failed" << endl;
	}
	else
	{
		if (sender())
		{
			if (sender()->isA("RateClass"))
				((RateClass *)(sender()))->dequeue();
		}
	}
}

// Logs in the user!
void OscarSocket::doLogin(
	const QString &host, int port,
	const QString &name, const QString &password,
	const QString &userProfile, const unsigned long initialStatus,
	const QString &/*awayMessage*/)
{
	QString realHost = host;

	if (isLoggedIn)
	{
		kdDebug(14151) << k_funcinfo << "We're already connected, aborting!" << endl;
		return;
	}
	if(realHost.isEmpty())
	{
		kdDebug(14151) << k_funcinfo << " Tried to connect without a hostname!" << endl;
		kdDebug(14151) << k_funcinfo << "Using login.oscar.aol.com" << endl;
		realHost = QString::fromLatin1("login.oscar.aol.com");
	}
	if(port < 1)
	{
		kdDebug(14151) << k_funcinfo << " Tried to connect to port < 1! Using port 5190" << endl;
		port = 5190;
	}
	if(password.isEmpty())
	{
		kdDebug(14151) << k_funcinfo << " Tried to connect without a password!" << endl;
		return;
	}

	kdDebug(14151) << k_funcinfo << "Connecting to '" << host << "', port=" << port << endl;

	disconnect(this, SIGNAL(socketConnected(const QString &)),
		this, SLOT(OnBosConnect()));
	disconnect(this, SIGNAL(connAckReceived()),
		this, SLOT(OnBosConnAckReceived()));

	connect(this, SIGNAL(socketConnected(const QString &)),
		this, SLOT(slotConnected()));
	connect(this, SIGNAL(connAckReceived()),
		this, SLOT(OnConnAckReceived()));

	setSN(name);
	loginPassword=password;
	loginProfile=userProfile;
	loginStatus=initialStatus;

	/*kdDebug(14151) << k_funcinfo <<
		"emitting statusChanged(OSCAR_CONNECTING)" << endl;*/
	emit statusChanged(OSCAR_CONNECTING);

	connectTo(realHost, QString::number(port));
}

void OscarSocket::parsePasswordKey(Buffer &inbuf)
{
	kdDebug(14151) << k_funcinfo << "Got the key" << endl;

	WORD keylen = inbuf.getWord();
	delete[] mPwEncryptionKey;
	mPwEncryptionKey = inbuf.getBlock(keylen);
	sendLoginAIM();
}

void OscarSocket::connectToBos()
{
	kdDebug(14151) << k_funcinfo <<
		"Cookie received! Connecting to BOS server..." << endl;

//	emit connectionChanged(4,"Connecting to server...");
	disconnect(this, SIGNAL(connAckReceived()),
		this, SLOT(OnConnAckReceived()));
	disconnect(this, SIGNAL(socketConnected(const QString &)),
		this, SLOT(slotConnected()));

	// NOTE: Disconnect from socketClosed signal so we do not set our
	// account status (and thus our icon) from connecting to offline
	disconnect(this, SIGNAL(socketClosed(const QString &, bool)),
		this, SLOT(slotConnectionClosed(const QString &, bool)));

	mSocket->reset();

	connect(this, SIGNAL(connAckReceived()),
		this, SLOT(OnBosConnAckReceived()));
	connect(this, SIGNAL(socketConnected(const QString &)),
		this, SLOT(OnBosConnect()));

	// NOTE: Reconnect socketClosed status to track if we (were) disconnected
	// from the main BOS server
	connect(this, SIGNAL(socketClosed(const QString &, bool)),
		this, SLOT(slotConnectionClosed(const QString &, bool)));

	connectTo(bosServer, QString::number(bosPort));
}

void OscarSocket::OnBosConnAckReceived()
{
	kdDebug(14151) << k_funcinfo <<
		"BOS server ack'ed us! Sending auth cookie" << endl;

	sendCookie();
//	emit connectionChanged(5,"Connected to server, authorizing...");
}

void OscarSocket::sendCookie()
{
	kdDebug(14151) << k_funcinfo << "SEND (CLI_COOKIE)" << endl;
	Buffer outbuf;
	putFlapVer(outbuf);
	outbuf.addTLV(0x0006, mCookieLength, mCookie);
	sendBuf(outbuf, 0x01);
}


void OscarSocket::OnBosConnect()
{
	kdDebug(14151) << k_funcinfo << "Connected to " << peerHost() <<
		", port " << peerPort() << endl;
}


void OscarSocket::parseAuthResponse(Buffer &inbuf)
{
	kdDebug(14151) << k_funcinfo << "Called." << endl;

	QPtrList<TLV> lst = inbuf.getTLVList();
	lst.setAutoDelete(TRUE);
	TLV *sn = findTLV(lst,0x0001);  //screen name
	TLV *url = findTLV(lst,0x0004);  //error url
	TLV *bosip = findTLV(lst,0x0005); //bos server address
	TLV *cook = findTLV(lst,0x0006); //authorization cookie
	//TLV *email = findTLV(lst,0x0007); //the email address attached to the account
	//TLV *regstatus = findTLV(lst,0x0013); //whether the email address is available to others
	TLV *err = findTLV(lst,0x0008); //whether an error occurred
	TLV *passChangeUrl = findTLV(lst,0x0054); //Password change URL, TODO: use for AIM accounts

	if (passChangeUrl)
	{
		kdDebug(14151) << k_funcinfo << "password change url='"  << passChangeUrl->data << "'" << endl;
		delete [] passChangeUrl->data;
	}

	delete[] mCookie;

	if (err)
	{
		QString errorString;
		int errorCode = (err->data[0] << 8)|err->data[1];
		delete[] err->data;

		parseAuthFailedCode(errorCode);
	}

	if (bosip)
	{
		QString ip = bosip->data;
		int index;
		index = ip.find(':');
		bosServer = ip.left(index);
		ip.remove(0,index+1); //get rid of the colon and everything before it
		bosPort = ip.toInt();

		kdDebug(14151) << "server is " << bosServer <<
			", ip.right(index) is " << ip <<
			", bosPort is " << bosPort << endl;

		delete[] bosip->data;
	}

	if (cook)
	{
		mCookie=cook->data;
		mCookieLength=cook->length;
		connectToBos();
	}

	if (sn)
		delete [] sn->data;

	if (url)
		delete [] url->data;

	lst.clear();
}


bool OscarSocket::parseAuthFailedCode(WORD errorCode)
{
	QString err;
	QString accountType = (mIsICQ ? i18n("ICQ") : i18n("AIM"));
	QString accountDescr = (mIsICQ ? "UIN" : "screen name");

	switch(errorCode)
	{
		case 0x0001:
		{
			if (isLoggedIn) // multiple logins (on same UIN)
			{
				err = i18n("You have logged in more than once with the same %1," \
					" account %2 is now disconnected.")
					.arg(accountDescr).arg(getSN());
			}
			else // error while logging in
			{
				emit wrongPassword();
				return true;
				/*
				err = i18n("Sign on failed because either your %1 or " \
					"password are invalid. Please check your settings for account %2.")
					.arg(accountDescr).arg(getSN());
				*/
			}
			break;
		}

		case 0x0002: // Service temporarily unavailable
		case 0x0014: // Reservation map error
		{
			err = i18n("The %1 service is temporarily unavailable. Please " \
				"try again later.").arg(accountType).arg(getSN());
			break;
		}

		case 0x0004: // Incorrect nick or password, re-enter
		case 0x0005: // Mismatch nick or password, re-enter
		{
			emit wrongPassword();
			return true;
			/*err = i18n("Could not sign on to %1 with account %2 as the " \
				"password was incorrect.").arg(accountType).arg(getSN());
			*/
			break;
		}

		case 0x0007: // non-existant ICQ#
		case 0x0008: // non-existant ICQ#
		{
			err = i18n("Could not sign on to %1 with nonexistent account %2.")
				.arg(accountType).arg(getSN());
			break;
		}

		case 0x0009: // Expired account
		{
			err = i18n("Sign on to %1 failed because your account %2 expired.")
				.arg(accountType).arg(getSN());
			break;
		}

		case 0x0011: // Suspended account
		{
			err = i18n("Sign on to %1 failed because your account %2 is " \
				"currently suspended.").arg(accountType).arg(getSN());
			break;
		}

		case 0x0015: // too many clients from same IP
		case 0x0016: // too many clients from same IP
		case 0x0017: // too many clients from same IP (reservation)
		{
			err = i18n("Could not sign on to %1 as there are too many clients" \
				" from the same computer.").arg(accountType);
			break;
		}

		case 0x0018: // rate exceeded (turboing)
		{
			if (isLoggedIn)
			{
				err = i18n("Account %1 was blocked on the %2 server for" \
					" sending messages too quickly." \
					" Wait ten minutes and try again." \
					" If you continue to try, you will" \
					" need to wait even longer.")
					.arg(getSN()).arg(accountType);
			}
			else
			{
				err = i18n("Account %1 was blocked on the %2 server for" \
					" reconnecting too quickly." \
					" Wait ten minutes and try again." \
					" If you continue to try, you will" \
					" need to wait even longer.")
					.arg(getSN()).arg(accountType);
			}
			break;
		}

		case 0x001C:
		{
			err = i18n("The %1 server thinks the client you are using is " \
				"too old. Please report this as a bug at http://bugs.kde.org")
				.arg(accountType);
			break;
		}

		case 0x0022: // Account suspended because of your age (age < 13)
		{
			err = i18n("Account %1 was disabled on the %2 server because " \
				"of your age (less than 13).")
			.arg(getSN()).arg(accountType);
			break;
		}

		default:
		{
			if (!isLoggedIn)
			{
				err = i18n("Sign on to %1 with your account %2 failed.")
					.arg(accountType).arg(getSN());
			}
			break;
		}
	}


	if (errorCode > 0)
		emit protocolError(err, errorCode, true);
	return (errorCode > 0);
}

TLV * OscarSocket::findTLV(QPtrList<TLV> &l, WORD typ)
{
	TLV *t;
	for(t=l.first();t;t=l.next())
	{
		if (t->type == typ)
			return t;
	}
	return NULL;
}



void OscarSocket::requestBOSRights(void)
{
	kdDebug(14151) << k_funcinfo << "SEND (CLI_REQBOS) Requesting BOS rights" << endl;
	Buffer outbuf;
	outbuf.addSnac(0x0009,0x0002,0x0000,0x00000002);
	sendBuf(outbuf,0x02);
}

void OscarSocket::parseBOSRights(Buffer &inbuf)
{
	kdDebug(14151) << k_funcinfo << "RECV (SRV_REPLYBOS) " << endl;

	QPtrList<TLV> ql = inbuf.getTLVList();
	ql.setAutoDelete(TRUE);
	TLV *t;
	WORD maxpermits = 0, maxdenies = 0;
	if ((t = findTLV(ql,0x0001))) //max permits
		maxpermits = (t->data[0] << 8) | t->data[1];
	if ((t = findTLV(ql,0x0002))) //max denies
		maxdenies = (t->data[0] << 8) | t->data[1];
//	kdDebug(14151) << k_funcinfo << "maxpermits=" << maxpermits << ", maxdenies=" << maxdenies << endl;
	ql.clear();
	//sendGroupPermissionMask();
	//sendPrivacyFlags();

	gotAllRights++;
	if (gotAllRights==7)
	{
		kdDebug(14151) << k_funcinfo "gotAllRights==7" << endl;
		sendInfo();
	}
}




// Requests location rights (CLI_REQLOCATION)
void OscarSocket::requestLocateRights()
{
	kdDebug(14151) << k_funcinfo << "SEND (CLI_REQLOCATION), Requesting rights for location service" << endl;
	Buffer buf;
//	buf.addSnac(0x0002,0x0002,0x0000,0x00000002);
	buf.addSnac(0x0002,0x0002,0x0000,0x00000000);
	sendBuf(buf,0x02);
}

/** adds a mask of the groups that you want to be able to see you to the buffer */
void OscarSocket::sendGroupPermissionMask()
{
	Buffer outbuf;
	outbuf.addSnac(0x0009,0x0004,0x0000,0x00000000);
	outbuf.addDWord(0x0000001f);
	sendBuf(outbuf,0x02);
}

// adds a request for buddy list rights to the buffer
void OscarSocket::requestBuddyRights()
{
	kdDebug(14151) << k_funcinfo <<
		"SEND (CLI_REQBUDDY), Requesting rights for BLM (local buddy list management)" << endl;
	Buffer outbuf;
	outbuf.addSnac(OSCAR_FAM_3,0x0002,0x0000,0x00000000);
	sendBuf(outbuf,0x02);
}

// Parses the locate rights provided by the server (SRV_REPLYLOCATION)
void OscarSocket::parseLocateRights(Buffer &/*inbuf*/)
{
	kdDebug(14151) << k_funcinfo << "RECV (SRV_REPLYLOCATION), TODO: Ignoring location rights" << endl;
	//we don't care what the locate rights are
	//and we don't know what they mean
	//  requestBuddyRights();

	gotAllRights++;
	if (gotAllRights==7)
	{
		kdDebug(14151) << k_funcinfo "gotAllRights==7" << endl;
		sendInfo();
	}
}

void OscarSocket::parseBuddyRights(Buffer &inbuf)
{
	kdDebug(14151) << k_funcinfo << "RECV (SRV_REPLYBUDDY), TODO: Ignoring Buddy Rights" << endl;
	// TODO: use these values if possible

	while(1)
	{
		TLV t = inbuf.getTLV();
		if(t.data == 0L)
			break;

		Buffer tlvBuf(t.data, t.length);
		switch(t.type)
		{
			case 0x0001:
				kdDebug(14151) << k_funcinfo <<
					"max contactlist size     = " << tlvBuf.getWord() << endl;
				break;
			case 0x0002:
				kdDebug(14151) << k_funcinfo <<
					"max no. of watchers      = " << tlvBuf.getWord() << endl;
				break;
			case 0x0003:
				kdDebug(14151) << k_funcinfo <<
					"max online notifications = " << tlvBuf.getWord() << endl;
				break;
			default:
				break;
		}
		tlvBuf.clear();
	}

	gotAllRights++;
	if (gotAllRights==7)
	{
		kdDebug(14151) << k_funcinfo "gotAllRights==7" << endl;
		sendInfo();
	}
}




// parses the standard user info block
bool OscarSocket::parseUserInfo(Buffer &inbuf, UserInfo &u)
{
	u.userclass=0;
	u.evil=0;
	u.idletime = 0;
	u.sessionlen = 0;
	u.localip = 0;
	u.realip = 0;
	u.port = 0;
	u.fwType = 0;
	u.version = 0;
	u.icqextstatus=0;
	u.capabilities=0;
	u.dcCookie=0;
	u.clientFeatures=0;
	u.lastInfoUpdateTime=0;
	u.lastExtInfoUpdateTime=0;
	u.lastExtStatusUpdateTime=0;

	if(inbuf.length() == 0)
	{
		kdDebug(14151) << k_funcinfo << "ZERO sized userinfo!" << endl;
		return false;
	}

	char *cb = inbuf.getBUIN(); // screenname/uin
	u.sn = tocNormalize(QString::fromLatin1(cb)); // screennames and uin are always us-ascii
	delete [] cb;

	// Next comes the warning level
	//for some reason aol multiplies the warning level by 10
	u.evil = (int)(inbuf.getWord() / 10);

	WORD tlvlen = inbuf.getWord(); //the number of TLV's that follow

	/*
	OscarContact *contact = static_cast<OscarContact*>(mAccount->contacts()[u.sn]);
	if (contact)
	{
		kdDebug(14151) << k_funcinfo
			<< "Contact: '" << contact->displayName() <<endl;
	}
	else
	{
		kdDebug(14151) << k_funcinfo
			<< "Contact: '" << u.sn << endl;
	}
	*/

	for (unsigned int i=0; i<tlvlen; i++)
	{
		TLV t = inbuf.getTLV();
		Buffer tlvBuf(t.data,t.length);

		switch(t.type)
		{
			case 0x0001: //user class
			{
				u.userclass = tlvBuf.getWord();
				break;
			}
			case 0x0002: //member since time
			case 0x0005: // member since time (again)
			{
				u.membersince.setTime_t(tlvBuf.getDWord());
				break;
			}
			case 0x0003: //online since time
			{
				u.onlinesince.setTime_t(tlvBuf.getDWord());
				break;
			}
			case 0x0004: //idle time
			{
				u.idletime = tlvBuf.getWord();
				break;
			}
			case 0x0006:
			{
				u.icqextstatus = tlvBuf.getDWord();
				break;
			}
			case 0x000a: // IP in a DWORD [ICQ]
			{
				u.realip = htonl(tlvBuf.getDWord());
				break;
			}
			case 0x000c: // CLI2CLI
			{
				u.localip = htonl(tlvBuf.getDWord()); // DC internal ip address
				u.port = tlvBuf.getDWord(); // DC tcp port
				u.fwType = tlvBuf.getByte(); // DC type
				u.version = tlvBuf.getWord(); // DC protocol version
				u.dcCookie = tlvBuf.getDWord(); // DC auth cookie
				u.webFrontPort = tlvBuf.getDWord(); // Web front port
				u.clientFeatures = tlvBuf.getDWord(); // Client features
				u.lastInfoUpdateTime = tlvBuf.getDWord(); // last info update time
				u.lastExtInfoUpdateTime = tlvBuf.getDWord(); // last ext info update time (i.e. icqphone status)
				u.lastExtStatusUpdateTime = tlvBuf.getDWord(); //last ext status update time (i.e. phonebook)
				tlvBuf.getWord(); // unknown
				break;
			}
			case 0x000d: //capability info
			{
				u.capabilities = parseCapabilities(tlvBuf, u.clientVersion);
				break;
			}
			case 0x0010: //session length (for AOL users, in seconds)
			case 0x000f: //session length (for AIM users, in seconds)
			{
				u.sessionlen = tlvBuf.getDWord();
				break;
			}
			case 0x001e: // unknown, empty
			{
				break;
			}
			default: // unknown info type
			{
				/*kdDebug(14151) << k_funcinfo << "Unknown TLV(" << t.type <<
					") length=" << t.length << " in userinfo for user '" <<
					u.sn << "'" << tlvBuf.toString() << endl;*/
			}
		}; // END switch()
		tlvBuf.clear(); // unlink tmpBuf from tlv data
		delete [] t.data; // get rid of tlv data.
	} // END for (unsigned int i=0; i<tlvlen; i++)


	if (u.capabilities != 0)
	{
		/**
		 * Client type detection ---
		 * Most of this code is based on sim-icq code
		 * Thanks a lot for all the tests you guys must have made
		 * without sim-icq I would have only checked for the capabilities
		 **/
		bool clientMatched = false;
		if (u.hasCap(CAP_KOPETE))
		{
			u.clientName=i18n("Kopete");
			clientMatched=true;
		}
		else if (u.hasCap(CAP_MICQ))
		{
			u.clientName=i18n("MICQ");
			clientMatched=true;
		}
		else if (u.hasCap(CAP_SIMNEW) || u.hasCap(CAP_SIMOLD))
		{
			u.clientName=i18n("SIM");
			clientMatched=true;
		}
		else if (u.hasCap(CAP_TRILLIANCRYPT) || u.hasCap(CAP_TRILLIAN))
		{
			u.clientName=i18n("Trillian");
			clientMatched=true;
		}
		else if (u.hasCap(CAP_MACICQ))
		{
			u.clientName=i18n("MacICQ");
			clientMatched=true;
		}
		else if ((u.lastInfoUpdateTime & 0xFF7F0000L) == 0x7D000000L)
		{
			unsigned ver = u.lastInfoUpdateTime & 0xFFFF;
			if (u.lastInfoUpdateTime & 0x00800000L)
				u.clientName=i18n("Licq SSL");
			else
				u.clientName=i18n("Licq");

			if (ver % 10)
				u.clientVersion.sprintf("%u.%u.%u", ver/1000, (ver/10)%100, ver%10);
			else
				u.clientVersion.sprintf("%u.%u", ver/1000, (ver/10)%100);
			clientMatched=true;
		}
		else // some client we could not detect using capabilities
		{
			/*kdDebug(14151) << k_funcinfo << "checking update times" << endl;
			kdDebug(14151) << k_funcinfo << u.lastInfoUpdateTime << endl;
			kdDebug(14151) << k_funcinfo << u.lastExtStatusUpdateTime << endl;
			kdDebug(14151) << k_funcinfo << u.lastExtInfoUpdateTime << endl;*/

			clientMatched=true; // default case will set it to false again if we did not find anything
			switch (u.lastInfoUpdateTime)
			{
				case 0xFFFFFFFFL:
					if ((u.lastExtStatusUpdateTime == 0xFFFFFFFFL) &&
						(u.lastExtInfoUpdateTime == 0xFFFFFFFFL))
					{
						u.clientName=QString::null; /*QString::fromLatin1("Gaim");*/
					}
					else
					{
						//kdDebug(14151) << k_funcinfo << "update times revealed MIRANDA" << endl;
						if (u.lastExtStatusUpdateTime & 0x80000000)
							u.clientName=QString::fromLatin1("Miranda alpha");
						else
							u.clientName=QString::fromLatin1("Miranda");

						DWORD version = (u.lastExtInfoUpdateTime & 0xFFFFFF);
						BYTE major1 = ((version >> 24) & 0xFF);
						BYTE major2 = ((version >> 16) & 0xFF);
						BYTE minor1 = ((version >> 8) & 0xFF);
						BYTE minor2 = (version & 0xFF);
						if (minor2 > 0) // w.x.y.z
						{
							u.clientVersion.sprintf("%u.%u.%u.%u", major1, major2,
								minor1, minor2);
						}
						else if (minor1 > 0)  // w.x.y
						{
							u.clientVersion.sprintf("%u.%u.%u", major1, major2, minor1);
						}
						else // w.x
						{
							u.clientVersion.sprintf("%u.%u", major1, major2);
						}
					}
					break;
				case 0xFFFFFF8FL:
					u.clientName=QString::fromLatin1("StrICQ");
					break;
				case 0xFFFFFF42L:
					u.clientName=QString::fromLatin1("mICQ");
					break;
				case 0xFFFFFFBEL:
					u.clientName=QString::fromLatin1("alicq");
					break;
				case 0xFFFFFF7FL:
					u.clientName=QString::fromLatin1("&RQ");
					break;
				case 0xFFFFFFABL:
					u.clientName=QString::fromLatin1("YSM");
					break;
				case 0x3AA773EEL:
					if ((u.lastExtStatusUpdateTime == 0x3AA66380L) &&
						(u.lastExtInfoUpdateTime == 0x3A877A42L))
					{
						u.clientName=QString::fromLatin1("libicq2000");
					}
					break;
				default:
				{
					/*kdDebug(14151) << k_funcinfo <<
						"Could not detect client from update times" << endl;*/
					clientMatched=false;
				}
			}
		}


		if (!clientMatched) // now the fuzzy clientsearch starts =)
		{
			/*kdDebug(14190) << k_funcinfo <<
					"Client fuzzy search..." << endl;*/
			if (u.hasCap(CAP_TYPING))
			{
				kdDebug(14190) << k_funcinfo <<
					"Client protocol version = " << u.version << endl;
				switch (u.version)
				{
					case 10:
						u.clientName=QString::fromLatin1("ICQ 2003b");
						break;
					case 9:
						u.clientName=QString::fromLatin1("ICQ Lite");
						break;
					default:
						u.clientName=QString::fromLatin1("ICQ2go");
				}
			}
			else if (u.hasCap(CAP_BUDDYICON)) // only gaim seems to advertize this on ICQ
			{
				u.clientName=QString::fromLatin1("Gaim");
			}
			else if (u.hasCap(CAP_XTRAZ))
			{
				u.clientName=QString::fromLatin1("ICQ 4.0 Lite");
			}
			else if ((u.hasCap(CAP_STR_2001) || u.hasCap(CAP_ICQSERVERRELAY)) &&
				u.hasCap(CAP_IS_2001))
			{
				u.clientName=QString::fromLatin1( "ICQ 2001");
			}
			else if ((u.hasCap(CAP_STR_2001) || u.hasCap(CAP_ICQSERVERRELAY)) &&
				u.hasCap(CAP_STR_2002))
			{
				u.clientName=QString::fromLatin1("ICQ 2002");
			}
			else if (u.hasCap(CAP_RTFMSGS) && u.hasCap(CAP_UTF8) &&
				u.hasCap(CAP_ICQSERVERRELAY) && u.hasCap(CAP_ISICQ))
			{
				u.clientName=QString::fromLatin1("ICQ 2003a");
			}
			else if (u.hasCap(CAP_ICQSERVERRELAY) && u.hasCap(CAP_ISICQ))
			{
				u.clientName=QString::fromLatin1("ICQ 2001b");
			}
			else if ((u.version == 7) && u.hasCap(CAP_RTFMSGS))
			{
				u.clientName=QString::fromLatin1("GnomeICU");
			}
		}
		//kdDebug(14151) << k_funcinfo << "|| clientName     = " << u.clientName << endl;
		//kdDebug(14151) << k_funcinfo << "|| clientVersion  = " << u.clientVersion << endl;
	}

	return true;
}

void OscarSocket::parseUserOnline(Buffer &inbuf)
{
	UserInfo u;
	if (parseUserInfo(inbuf, u))
	{
		//kdDebug(14151) << k_funcinfo << "RECV SRV_USERONLINE, name=" << u.sn << endl;
		emit gotContactChange(u);
	}
}

void OscarSocket::parseUserOffline(Buffer &inbuf)
{
	UserInfo u;
	if (parseUserInfo(inbuf, u))
	{
		//kdDebug(14151) << k_funcinfo << "RECV SRV_USEROFFLINE, name=" << u.sn << endl;
		emit gotOffgoingBuddy(u.sn);
	}
}


void OscarSocket::sendChangePassword(const QString &newpw, const QString &oldpw)
{
	// FIXME: This does not work :-(

	kdDebug(14151) << k_funcinfo << "Changing password from " << oldpw << " to " << newpw << endl;
	Buffer outbuf;
	outbuf.addSnac(0x0007,0x0004,0x0000,0x00000000);
	outbuf.addTLV(0x0002,newpw.length(),newpw.latin1());
	outbuf.addTLV(0x0012,oldpw.length(),oldpw.latin1());
	sendBuf(outbuf,0x02);
}


#if 0
void OscarSocket::parseRedirect(Buffer &inbuf)
{
	// FIXME: this function does not work, but it is never
	// called unless you want to connect to the advertisements server
	kdDebug(14151) << "Parsing redirect" << endl;

	OscarConnection *servsock = new OscarConnection(getSN(),"Redirect",Redirect,QByteArray(8));

	QPtrList<TLV> tl = inbuf.getTLVList();
//	int n;
	QString host;
	tl.setAutoDelete(true);

	if (!findTLV(tl,0x0006) && !findTLV(tl,0x0005) && !findTLV(tl,0x000e))
	{
		tl.clear();
		emit protocolError(
			i18n("An unknown error occurred. Please check " \
				"your internet connection. The error message " \
				"was: \"Not enough information found in server redirect\""), 0);
		return;
	}
	for (TLV *tmp = tl.first(); tmp; tmp = tl.next())
	{
		switch (tmp->type)
		{
			case 0x0006: //auth cookie
/*				for (int i=0;i<tmp->length;i++)
					servsock->mAuthCookie[i] = tmp->data[i];*/
				break;
			case 0x000d: //service type
				//servsock->mConnType = (tmp->data[1] << 8) | tmp->data[0];
				break;
			case 0x0005: //new ip & port
			{
/*				host = tmp->data;
				n = host.find(':');
				if (n != -1)
				{
					servsock->mHost = host.left(n);
					servsock->mPort = host.right(n).toInt();
				}
				else
				{
					servsock->mHost = host;
					servsock->mPort = peerPort();
				}
				kdDebug(14151) << "Set host to " << servsock->mHost <<
					", port to " << servsock->mPort << endl;*/
				break;
			}

			default: //unknown
				kdDebug(14151) << "Unknown tlv type in parseredirect: " << tmp->type << endl;
				break;
		}
		delete [] tmp->data;
	}
	tl.clear();
	//sockets.append(servsock);
	delete servsock;
	kdDebug(14151) << "Socket added to connection list!" << endl;
}
#endif




void OscarSocket::sendLocationInfo(const QString &profile)
{
	kdDebug(14151) << k_funcinfo << "SEND (CLI_SETUSERINFO/CLI_SET_LOCATION_INFO)" << endl;
	Buffer outbuf, capBuf;

	outbuf.addSnac(OSCAR_FAM_2,0x0004,0x0000,0x00000000);
	if(!profile.isNull() && !mIsICQ)
	{
		static const QString defencoding = "text/aolrtf; charset=\"us-ascii\"";
		outbuf.addTLV(0x0001, defencoding.length(), defencoding.latin1());
		outbuf.addTLV(0x0002, profile.length(), profile.local8Bit());
		//kdDebug(14151) << k_funcinfo << "adding profile=" << profile << endl;
	}

	if (mIsICQ)
	{
		capBuf.addString(oscar_caps[CAP_ICQSERVERRELAY], 16); // we support type-2 messages
		capBuf.addString(oscar_caps[CAP_UTF8], 16); // we can send/receive UTF encoded messages
		capBuf.addString(oscar_caps[CAP_ISICQ], 16); // I think this is an icq client, but maybe I'm wrong
		capBuf.addString(oscar_caps[CAP_KOPETE], 16); // we are the borg, resistance is futile
		capBuf.addString(oscar_caps[CAP_RTFMSGS], 16); // we do incoming RTF messages
		capBuf.addString(oscar_caps[CAP_TYPING], 16); // we can send/receive typing notifications
	}
	else
	{
		capBuf.addString(oscar_caps[CAP_KOPETE], 16); // we are the borg, resistance is futile
	}

	//kdDebug(14151) << k_funcinfo << "adding capabilities, size=" << capBuf.length() << endl;
	outbuf.addTLV(0x0005, capBuf.length(), capBuf.buffer());

	sendBuf(outbuf,0x02);
}


void OscarSocket::doLogoff()
{
	kdDebug(14151) << k_funcinfo << endl;

	mSSIData.clear();
	if(isLoggedIn && (socketStatus() == OscarConnection::Connected))
	{
		kdDebug(14151) << k_funcinfo << "Sending sign off request" << endl;
		Buffer outbuf;
		sendBuf(outbuf, 0x04);
		close();
	}
	else
	{
		if(socketStatus() == OscarConnection::Connecting ||
			socketStatus() == OscarConnection::Connected )
		{
			kdDebug(14151) << k_funcinfo << endl <<
				" === ERROR ===" << endl <<
				"We are either not logged in correctly or something" <<
				"wicked happened, closing down socket..." << endl <<
				" === ERROR ===" << endl;

			//mSocket->close();
			mSocket->reset();

			// FIXME ?
			// The above socket calls _should_ trigger a connectionClosed,
			// I hope it really does [mETz, 05-05-2004]
			//emit connectionClosed(QString::null);
		}
	}
}


// Gets this user status through BLM service
void OscarSocket::sendAddBuddylist(const QString &contactName)
{
	kdDebug(14151) << k_funcinfo << "Sending BLM add buddy" << endl;
	QStringList Buddy = contactName;
	sendBuddylistAdd(Buddy);
}

void OscarSocket::sendDelBuddylist(const QString &contactName)
{
	kdDebug(14151) << k_funcinfo << "Sending BLM del buddy" << endl;
	QStringList Buddy = contactName;
	sendBuddylistDel(Buddy);
}



void OscarSocket::parseError(WORD family, WORD snacID, Buffer &inbuf)
{
	QString msg;
	WORD reason = inbuf.getWord();
	kdDebug(14151) << k_funcinfo <<
		"Got an error, SNAC family=" << family << ", reason=" << reason << endl;


	if (reason < msgerrreasonlen)
	{
		switch (family)
		{
			case OSCAR_FAM_2:
			{
				// Ignores recipient is not logged in errors, usually caused by querying aim userinfo
				if (reason == 4)
				{
					kdDebug(14151) << k_funcinfo <<
						"IGNORED Family 2 error, recipient not logged in" << endl;
					return;
				}
				msg = i18n("Sending userprofile for account %1 failed because " \
					"the following error occurred:\n%2")
					.arg(getSN(), msgerrreason[reason]);
				break;
			}
			case OSCAR_FAM_4:
			{
				// Ignores rate to client errors, usually caused by querying away messages
				if (reason == 3)
				{
					kdDebug(14151) << k_funcinfo <<
						"IGNORED Family 4 error, rate to client" << endl;
					return;
				}
				msg = i18n("Your message for account %1 did not get sent because" \
					" the following error occurred: %2")
					.arg(getSN(), msgerrreason[reason]);
				break;
			}
			case OSCAR_FAM_21: // ICQ specific packets
			{
				if (reason == 2)
				{
					msg = i18n("Your ICQ information request for account %1 was"
						" denied by the ICQ-Server, please try again later.")
						.arg(getSN());
				}
				else
				{
					msg = i18n("Your ICQ information request for account %1 failed " \
						"because of the following error:\n%2")
						.arg(getSN(), msgerrreason[reason]);
				}
				break;
			}
			default:
			{
				msg = i18n("Generic Packet error for account %1:\n%2")
					.arg(getSN(), msgerrreason[reason]);
				break;
			}
		}
	}
	else
	{
		if (family == OSCAR_FAM_2)
		{
			msg = i18n("Sending userprofile for account %1 failed: Unknown Error.\n" \
				"Please report a bug at http://bugs.kde.org").arg(getSN());
		}
		else if (family == OSCAR_FAM_4)
		{
			msg = i18n("Your message for account %1 did not get sent: Unknown Error.\n" \
				"Please report a bug at http://bugs.kde.org").arg(getSN());
		}
		else
		{
			msg = i18n("Generic Packet error for account %1: Unknown Error.\n" \
				"Please report a bug at http://bugs.kde.org").arg(getSN());
		}
	}

	emit protocolError(msg, reason, false);
	// in case a special request failed
	emit snacFailed(snacID);
}



void OscarSocket::sendInfo()
{
	//kdDebug(14151) << k_funcinfo << "Called." << endl;

	// greater 7 and thus sendInfo() is not getting called again
	// except on reconnnect
	gotAllRights=99;

	sendLocationInfo(loginProfile); // CLI_SETUSERINFO
	loginProfile = QString::null;

	sendMsgParams(); // CLI_SETICBM

	sendIdleTime(0); // CLI_SNAC1_11, sent before CLI_SETSTATUS

	sendICQStatus(loginStatus); // send initial login status in case we are ICQ

	if (!mIsICQ)
	{
		sendGroupPermissionMask(); // unknown to icq docs
		sendPrivacyFlags(); // unknown to icq docs
	}

	sendClientReady(); // CLI_READY
	sendReqOfflineMessages(); // CLI_REQOFFLINEMSGS
	startKeepalive();

	requestMyUserInfo();
}


// Reads a FLAP header from the input
FLAP OscarSocket::getFLAP()
{
	BYTE start = 0x00;
	char peek[6];
	FLAP fl;
	fl.error = false;

	if (mSocket->peekBlock(&peek[0], 6) != 6 )
	{
		kdDebug(14151) << k_funcinfo <<
			"Error reading 6 bytes for FLAP" << endl;
		fl.error = true;
		return fl;
	}

	Buffer buf(&peek[0], 6);
	start = buf.getByte();

	if (start == 0x2a)
	{
		//get the channel ID
		fl.channel = buf.getByte();
		//kdDebug(14151) << k_funcinfo << "FLAP channel=" << fl.channel << endl;

		//get the sequence number
		fl.sequence_number = buf.getWord();
		//kdDebug(14151) << k_funcinfo << "FLAP sequence_number=" << fl.sequence_number << endl;

		// get the packet length
		fl.length = buf.getWord();
		//kdDebug(14151) << k_funcinfo << "FLAP length=" << fl.length << endl;
		//kdDebug(14151) << k_funcinfo << "bytes available=" << mSocket->bytesAvailable() << endl;

		if(mSocket->bytesAvailable() < fl.length+6)
		{
			kdDebug(14151) << k_funcinfo <<
				"Not enough data in recv buffer to read the full FLAP (want " <<
				fl.length+6 << " bytes, got " << mSocket->bytesAvailable() <<
				"bytes), aborting" << endl;
			fl.error = true;
		}
	}
	else
	{
		kdDebug(14151) << k_funcinfo <<
			"Error reading FLAP, start byte is '" << start << "'" << endl;
		fl.error = true;
	}

	// get rid of FLAP header because we successfully retrieved it
	if (!fl.error)
	{
		/*kdDebug(14151) << k_funcinfo <<
			"Returning FLAP, channel=" << fl.channel << ", length=" << fl.length << endl;*/
		mSocket->readBlock(0L, 6);
	}
	return fl;
}



#if 0
// Called when a direct IM is received
void OscarSocket::OnDirectIMReceived(QString message, QString sender, bool isAuto)
{
	kdDebug(14151) << k_funcinfo << "Called." << endl;

	OscarMessage oMsg;
	oMsg.setText(message, OscarMessage::Plain);
	oMsg.setType(isAuto ? OscarMessage::Away : OscarMessage::Normal);

	emit receivedMessage(sender, oMsg);
}

// Called when a direct IM connection suffers an error
void OscarSocket::OnDirectIMError(QString errmsg, int num)
{
	//let's just emit a protocolError for now
	emit protocolError(errmsg, num);
}

// Called whenever a direct IM connection gets a typing notification
void OscarSocket::OnDirectMiniTypeNotification(QString screenName, int notify)
{
	//for now, just emit a regular typing notification
	emit gotMiniTypeNotification(screenName, notify);
}

// Called when a direct IM connection bites the dust
void OscarSocket::OnDirectIMConnectionClosed(QString name)
{
	emit directIMConnectionClosed(name);
}

// Called when a direct connection is set up and ready for use
void OscarSocket::OnDirectIMReady(QString name)
{
	emit connectionReady(name);
}

void OscarSocket::sendFileSendRequest(const QString &sn, const KFileItem &finfo)
{
	kdDebug(14151) << k_funcinfo << "Called." << endl;
	sendRendezvous(sn, 0x0000, CAP_SENDFILE, &finfo);
}

OscarConnection * OscarSocket::sendFileSendAccept(const QString &sn,
	const QString &fileName)
{
	sendRendezvous(sn, 0x0001, CAP_SENDFILE);
	mFileTransferMgr->addFileInfo(sn,
		new KFileItem(KFileItem::Unknown, KFileItem::Unknown, fileName));

	return mFileTransferMgr->establishOutgoingConnection(sn);
}

void OscarSocket::sendFileSendDeny(const QString &sn)
{
	sendRendezvous(sn, 0x0002, CAP_SENDFILE);
}

void OscarSocket::OnFileTransferBegun(OscarConnection *con, const QString& file,
	const unsigned long size, const QString &recipient)
{
	kdDebug(14151) << k_funcinfo << "emitting transferBegun()" << endl;
	emit transferBegun(con, file, size, recipient);
}

OncomingSocket *OscarSocket::serverSocket(DWORD capflag)
{
	if ( capflag & CAP_IMIMAGE ) //direct im
		return mDirectIMMgr;
	else  //must be a file transfer?
		return mFileTransferMgr;
}
#endif



void OscarSocket::parseConnectionClosed(Buffer &inbuf)
{
	kdDebug(14151) << k_funcinfo << "RECV (DISCONNECT)" << endl;

	// This is a part of icq login procedure,
	// Move this into its own function!
	QPtrList<TLV> lst=inbuf.getTLVList();
	lst.setAutoDelete(TRUE);
/*
	kdDebug(14151) << "contained TLVs:" << endl;
	TLV *t;
	for(t=lst.first(); t; t=lst.next())
	{
		kdDebug(14151) << "TLV(" << t->type << ") with length " << t->length << endl;
	}
*/
	TLV *uin=findTLV(lst,0x0001);
	if(uin)
	{
		kdDebug(14151) << k_funcinfo << "found TLV(1) [UIN], uin=" << uin->data << endl;
		delete [] uin->data;
	}

	TLV *descr=findTLV(lst,0x0004);
	if(!descr)
		descr=findTLV(lst,0x000b);
	if(descr)
	{
		kdDebug(14151) << k_funcinfo << "found TLV(4) [DESCRIPTION] reason=" << descr->data << endl;
		delete [] descr->data;
	}

	TLV *err=findTLV(lst,0x0008);
	if (!err)
		err=findTLV(lst,0x0009);
	if (err)
	{
		WORD errorNum = ((err->data[0] << 8)|err->data[1]);
		delete [] err->data;

		kdDebug(14151) << k_funcinfo << k_funcinfo << "found TLV(8) [ERROR] error= " <<
			errorNum << endl;

		bool disc = parseAuthFailedCode(errorNum);
		if (disc)
			mAccount->disconnect(Kopete::Account::Manual); //doLogoff();
	}

	TLV *server = findTLV(lst,0x0005);
	if (server)
	{
		kdDebug(14151) << k_funcinfo << "found TLV(5) [SERVER]" << endl;
		QString ip=server->data;
		int index=ip.find(':');
		bosServer=ip.left(index);
		ip.remove(0,index+1); //get rid of the colon and everything before it
		bosPort=ip.toInt();
		kdDebug(14151) << k_funcinfo << "We should reconnect to server '" << bosServer <<
			"' on port " << bosPort << endl;
		delete[] server->data;
	}

	TLV *cook=findTLV(lst,0x0006);
	if (cook)
	{
		kdDebug(14151) << "found TLV(6) [COOKIE]" << endl;
		mCookie=cook->data;
		mCookieLength=cook->length;
//		delete [] cook->data;
		connectToBos();
	}
	lst.clear();
}


void OscarSocket::sendAuthRequest(const QString &contact, const QString &reason)
{
	kdDebug(14151) << k_funcinfo << "contact='" << contact << "', reason='" << reason <<
		"'" << endl;

	Buffer outbuf;
	outbuf.addSnac(0x0013,0x0018,0x0000,0x00000000);

	outbuf.addBUIN(contact.ascii()); //dest sn
	outbuf.addBSTR(reason.local8Bit());
	outbuf.addWord(0x0000);
	sendBuf(outbuf,0x02);
}


void OscarSocket::sendAuthReply(const QString &contact, const QString &reason, bool grant)
{
	kdDebug(14151) << k_funcinfo << "contact='" << contact << "', reason='" << reason <<
		"', grant=" << grant << endl;

	Buffer outbuf;
	outbuf.addSnac(0x0013,0x001a,0x0000,0x00000000);
	outbuf.addBUIN(contact.ascii()); //dest sn
	outbuf.addByte(grant ? 0x01 : 0x00);
	outbuf.addBSTR(reason.local8Bit());
	sendBuf(outbuf,0x02);
}


void OscarSocket::parseAuthReply(Buffer &inbuf)
{
	kdDebug(14151) << k_funcinfo << "Called." << endl;

	char *tmp = 0L;
	BYTE grant = 0;

	tmp = inbuf.getBUIN();
	QString contact = ServerToQString(tmp, 0L, false);
	delete [] tmp;

	grant = inbuf.getByte();

	tmp = inbuf.getBSTR();
	QString reason = ServerToQString(tmp, 0L, false);
	delete [] tmp;

	emit gotAuthReply(contact, reason, (grant==0x01));
}


void OscarSocket::sendBuddylistAdd(QStringList &contacts)
{
	kdDebug(14151) << k_funcinfo << "SEND CLI_ADDCONTACT (add to local userlist)" << endl;

	Buffer outbuf;
	outbuf.addSnac(OSCAR_FAM_3,0x0004,0x0000,0x00000000);
	for(QStringList::Iterator it = contacts.begin(); it != contacts.end(); ++it)
	{
		QCString contact = (*it).latin1();
		outbuf.addByte(contact.length());
		outbuf.addString(contact, contact.length());
	}
	sendBuf(outbuf,0x02);
}


void OscarSocket::sendBuddylistDel(QStringList &contacts)
{
	kdDebug(14151) << k_funcinfo << "SEND CLI_DELCONTACT (delete from local userlist)" << endl;

	Buffer outbuf;
	outbuf.addSnac(OSCAR_FAM_3,0x0005,0x0000,0x00000000);
	for(QStringList::Iterator it = contacts.begin(); it != contacts.end(); ++it)
	{
		QCString contact = (*it).latin1();
		outbuf.addByte(contact.length());
		outbuf.addString(contact, contact.length());
	}
	sendBuf(outbuf,0x02);
}


const QString OscarSocket::ServerToQString(const char* string, OscarContact *contact, bool isUtf8, bool isRTF)
{
	//kdDebug(14151) << k_funcinfo << "called" << endl;

	int length = strlen(string);
	int cresult = -1;
	QTextCodec *codec = 0L;

	/* TODO check how encodings are handled inside RTF,
	 * I assume it's part of the rtf and I do not have to decode it here
	 */
	if (isRTF)
		return QString::fromLatin1(string);


	if(contact != 0L)
	{
		if(contact->encoding() != 0)
			codec = QTextCodec::codecForMib(contact->encoding());
		else
			codec = 0L;
#ifdef CHARSET_DEBUG
		if(codec)
		{
			kdDebug(14151) << k_funcinfo <<
				"using per-contact encoding, MIB=" << contact->encoding() << endl;
		}
#endif
	}

	// in case the per-contact codec didn't work we check if the caller
	// thinks this message is utf8
	if(!codec && isUtf8)
	{
		codec = QTextCodec::codecForMib(106); //UTF-8
		if(codec)
		{
			cresult = codec->heuristicContentMatch(string, length);
#ifdef CHARSET_DEBUG
			kdDebug(14151) << k_funcinfo <<
				"result for FORCED UTF-8 = " << cresult <<
				", message length = " << length << endl;
#endif
			/*if(cresult < (length/2)-1)
				codec = 0L;*/
		}
	}

	return Kopete::Message::decodeString( string, codec );
}





bool UserInfo::hasCap(int capNumber)
{
	return (capabilities & (1 << capNumber)) != 0;
}

void UserInfo::updateInfo(UserInfo other)
{
	sn = other.sn;
	evil = other.evil;

	if (other.userclass != 0)
		userclass = other.userclass;

	idletime = other.idletime;
	onlinesince = other.onlinesince;
	icqextstatus = other.icqextstatus;

	if(other.capabilities != 0)
	{
		//kdDebug(14151)<< k_funcinfo << "Merging capabilities" << endl;
		capabilities = other.capabilities;
		clientName = other.clientName;
		clientVersion = other.clientVersion;
	}

	// TLV 0x0C, optional
	if(other.localip != 0 && other.port != 0 && other.version != 0)
	{
		//kdDebug(14151)<< k_funcinfo << "Merging DC info" << endl;
		localip = other.version;
		port = other.version;
		fwType = other.version;
		version = other.version;
		dcCookie = other.dcCookie;
		clientFeatures = other.clientFeatures;
		lastInfoUpdateTime = other.lastInfoUpdateTime;
		lastExtInfoUpdateTime = other.lastExtInfoUpdateTime;
		lastExtStatusUpdateTime = other.lastExtStatusUpdateTime;
	}

	// TLV 0x0A, optional
	if (other.realip != 0)
		realip = other.realip;
}

#include "oscarsocket.moc"
// vim: set noet ts=4 sts=4 sw=4:
