/*
    oscarsocket.h  -  Oscar Protocol Implementation

    Copyright (c) 2002 by Tom Linsky <twl6@po.cwru.edu>
    Kopete    (c) 2002-2003 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef OSCARSOCKET_H
#define OSCARSOCKET_H

#include "aim.h" // tocNormalize()
#include "oscardirectconnection.h"
#include "ssidata.h"
#include "aimbuddylist.h"
#include "oncomingsocket.h"
#include "oscarsocket.icq.h"

#include <klocale.h>

#include <qptrlist.h>
#include <qdatetime.h>

class KFileItem;
class OscarAccount;
class OscarContact;
class QTimer;
class KExtendedSocket;

struct FLAP
{
	//flap header
	 BYTE channel;
	 WORD sequence_number;
	 WORD length;
	 bool error;
};

struct SnacPair
{
	//just a group+type pair
	 WORD group;
	 WORD type;
};

struct RateClass
{
	//rate info
	WORD classid;
	DWORD windowsize;
	DWORD clear;
	DWORD alert;
	DWORD limit;
	DWORD disconnect;
	DWORD current;
	DWORD max;
	DWORD lastTime;
	BYTE currentState;
	QPtrList<SnacPair> members;
};

class UserInfo
{
	public:
		QString sn;
		int evil;
		int userclass;
		QDateTime membersince;
		QDateTime onlinesince;
		DWORD capabilities;
		long sessionlen;
		unsigned int idletime;
		unsigned long realip;
		unsigned long localip;
		unsigned int  port;
		unsigned int  fwType;
		unsigned int version;
		unsigned long icqextstatus;
};


const DWORD AIM_CAPS_BUDDYICON 		= 0x00000001;
const DWORD AIM_CAPS_VOICE			= 0x00000002;
const DWORD AIM_CAPS_IMIMAGE		= 0x00000004;
const DWORD AIM_CAPS_CHAT			= 0x00000008;
const DWORD AIM_CAPS_GETFILE		= 0x00000010;
const DWORD AIM_CAPS_SENDFILE		= 0x00000020;
const DWORD AIM_CAPS_GAMES			= 0x00000040;
const DWORD AIM_CAPS_SAVESTOCKS		= 0x00000080;
const DWORD AIM_CAPS_SENDBUDDYLIST	= 0x00000100;
const DWORD AIM_CAPS_GAMES2			= 0x00000200;
const DWORD AIM_CAPS_ISICQ			= 0x00000400;
const DWORD AIM_CAPS_APINFO			= 0x00000800;
const DWORD AIM_CAPS_RTFMSGS		= 0x00001000;
const DWORD AIM_CAPS_EMPTY			= 0x00002000;
const DWORD AIM_CAPS_ICQSERVERRELAY	= 0x00004000;
const DWORD AIM_CAPS_IS_2001		= 0x00008000;
const DWORD AIM_CAPS_TRILLIANCRYPT	= 0x00010000;
const DWORD AIM_CAPS_UTF8			= 0x00020000;
const DWORD AIM_CAPS_IS_WEB			= 0x00040000;
const DWORD AIM_CAPS_INTEROPERATE	= 0x00080000;
const DWORD AIM_CAPS_LAST			= 0x00100000;

// DON'T touch these if you're not 100% sure what they are for!
//#define KOPETE_AIM_CAPS			AIM_CAPS_IMIMAGE | AIM_CAPS_SENDFILE | AIM_CAPS_GETFILE
#define KOPETE_AIM_CAPS			0 // our aim client is as stupid as bread
#define KOPETE_ICQ_CAPS			/*AIM_CAPS_ICQSERVERRELAY |*/ AIM_CAPS_UTF8 | AIM_CAPS_ISICQ

//ICQ 2002b sends: CAP_AIM_SERVERRELAY, CAP_UTF8, CAP_RTFMSGS, CAP_AIM_ISICQ

//
// FIXME: port capabilities array to some qt based list class, makes usage of memcmp obsolete
//
const struct
{
	DWORD flag;
	unsigned char data[16];
} oscar_caps[] =
{
	// Chat is oddball.
	{AIM_CAPS_CHAT,
	{0x74, 0x8f, 0x24, 0x20, 0x62, 0x87, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	// These are mostly in order.
	{AIM_CAPS_VOICE,
	{0x09, 0x46, 0x13, 0x41, 0x4c, 0x7f, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_SENDFILE,
	{0x09, 0x46, 0x13, 0x43, 0x4c, 0x7f, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	// Advertised by the EveryBuddy client.
	{AIM_CAPS_ISICQ,
	{0x09, 0x46, 0x13, 0x44, 0x4c, 0x7f, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_IMIMAGE,
	{0x09, 0x46, 0x13, 0x45, 0x4c, 0x7f, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_BUDDYICON,
	{0x09, 0x46, 0x13, 0x46, 0x4c, 0x7f, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_SAVESTOCKS,
	{0x09, 0x46, 0x13, 0x47, 0x4c, 0x7f, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_GETFILE,
	{0x09, 0x46, 0x13, 0x48, 0x4c, 0x7f, 0x11, 0xd1,
		0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_ICQSERVERRELAY,
	{0x09, 0x46, 0x13, 0x49, 0x4c, 0x7f, 0x11, 0xd1,
	 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_GAMES,
	{0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
	 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_GAMES2,
	{0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
	 0x22, 0x82, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_SENDBUDDYLIST,
	{0x09, 0x46, 0x13, 0x4b, 0x4c, 0x7f, 0x11, 0xd1,
	 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_RTFMSGS,
	{0x97, 0xb1, 0x27, 0x51, 0x24, 0x3c, 0x43, 0x34,
	 0xad, 0x22, 0xd6, 0xab, 0xf7, 0x3f, 0x14, 0x92}},

	{AIM_CAPS_IS_2001,
	{0x2e, 0x7a, 0x64, 0x75, 0xfa, 0xdf, 0x4d, 0xc8,
	 0x88, 0x6f, 0xea, 0x35, 0x95, 0xfd, 0xb6, 0xdf}},

	{AIM_CAPS_EMPTY,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},

	{AIM_CAPS_TRILLIANCRYPT,
	{0xf2, 0xe7, 0xc7, 0xf4, 0xfe, 0xad, 0x4d, 0xfb,
	 0xb2, 0x35, 0x36, 0x79, 0x8b, 0xdf, 0x00, 0x00}},

	{AIM_CAPS_APINFO,
	{0xAA, 0x4A, 0x32, 0xB5, 0xF8, 0x84, 0x48, 0xc6,
	 0xA3, 0xD7, 0x8C, 0x50, 0x97, 0x19, 0xFD, 0x5B}},

	{AIM_CAPS_UTF8,
	{0x09, 0x46, 0x13, 0x4E, 0x4C, 0x7F, 0x11, 0xD1,
	 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_IS_WEB,
	{0x56, 0x3F, 0xC8, 0x09, 0x0B, 0x6f, 0x41, 0xBD,
	 0x9F, 0x79, 0x42, 0x26, 0x09, 0xDF, 0xA2, 0xF3}},

	{AIM_CAPS_INTEROPERATE,
	{0x09, 0x46, 0x13, 0x4D, 0x4C, 0x7F, 0x11, 0xD1,
	 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_LAST,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
};


const QString msgerrreason[] =
{
	I18N_NOOP("Invalid error"),
	I18N_NOOP("Invalid SNAC"),
	I18N_NOOP("Rate to host"),
	I18N_NOOP("Rate to client"),
	I18N_NOOP("Not logged in"),
	I18N_NOOP("Service unavailable"),
	I18N_NOOP("Service not defined"),
	I18N_NOOP("Obsolete SNAC"),
	I18N_NOOP("Not supported by host"),
	I18N_NOOP("Not supported by client"),
	I18N_NOOP("Refused by client"),
	I18N_NOOP("Reply too big"),
	I18N_NOOP("Responses lost"),
	I18N_NOOP("Request denied"),
	I18N_NOOP("Busted SNAC payload"),
	I18N_NOOP("Insufficient rights"),
	I18N_NOOP("In local permit/deny"),
	I18N_NOOP("Too evil (sender)"),
	I18N_NOOP("Too evil (receiver)"),
	I18N_NOOP("User temporarily unavailable"),
	I18N_NOOP("No match"),
	I18N_NOOP("List overflow"),
	I18N_NOOP("Request ambiguous"),
	I18N_NOOP("Queue full"),
	I18N_NOOP("Not while on AOL")
};

const int msgerrreasonlen=25;

//#define DIRECTCONNECT				0x0f1f // WHAT IS THAT FOR??? [mETz, 21.05.2003]

/*
 * Internal status enum
 */
const unsigned int OSCAR_OFFLINE = 0;
const unsigned int OSCAR_ONLINE = 1;
const unsigned int OSCAR_AWAY = 2;
const unsigned int OSCAR_DND = 3;
const unsigned int OSCAR_NA = 4;
const unsigned int OSCAR_OCC = 5;
const unsigned int OSCAR_FFC = 6;
const unsigned int OSCAR_CONNECTING = 10;

const WORD OSCAR_FAM_1			= 0x0001; // Services
const WORD OSCAR_FAM_2			= 0x0002; // Location
const WORD OSCAR_FAM_3			= 0x0003; // Contacts, adding, removal, statuschanges
const WORD OSCAR_FAM_4			= 0x0004; // ICBM, messaging
const WORD OSCAR_FAM_9			= 0x0009; // BOS, visible/invisible lists
const WORD OSCAR_FAM_11			= 0x000b; // Interval
const WORD OSCAR_FAM_19			= 0x0013; // Roster, Contactlist
const WORD OSCAR_FAM_21			= 0x0015; // icq metasearch, sms, offline messages
const WORD OSCAR_FAM_23			= 0x0017; // new user, registration


// used for SRV_RECVMSG, SNAC(4,7)
const WORD MSGFORMAT_SIMPLE		= 0x0001;
const WORD MSGFORMAT_ADVANCED	= 0x0002;
const WORD MSGFORMAT_SERVER		= 0x0004;

const char OSCAR_SERVER[] 		= "login.oscar.aol.com";
const unsigned int OSCAR_PORT	= 5190;

const WORD CLASS_TRIAL			= 0x0001;
const WORD CLASS_ADMINISTRATOR	= 0x0002; // AOL admin
const WORD CLASS_AOL			= 0x0004; // AOL staff user flag
const WORD CLASS_COMMERCIAL		= 0x0008; // AOL commercial account flag
const WORD CLASS_AIM			= 0x0010; // ICQ non-commercial account flag
const WORD CLASS_AWAY			= 0x0020; //  Away status flag
const WORD CLASS_ICQ			= 0x0040; //  ICQ user sign
const WORD CLASS_WIRELESS		= 0x0080; // AOL wireless user
const WORD CLASS_UNKNOWN100		= 0x0100; // Unknown bit
const WORD CLASS_UNKNOWN200		= 0x0200;  // Unknown bit
const WORD CLASS_UNKNOWN400		= 0x0400;  // Unknown bit
//const WORD CLASS_ACTIVEBUDDY	= 0x0400;
const WORD CLASS_UNKNOWN800		= 0x0800; // Unknown bit

/*
 * Implements the actual communication with the oscar server
 * @author Tom Linsky
 * @author Stefan Gehn
*/
class OscarSocket : public OscarConnection
{
	Q_OBJECT

	public:
		enum OscarMessageType
		{
			Normal=0, Away, URL, SMS, WebPanel, GrantedAuth, DeclinedAuth
		};

		OscarSocket(const QString &connName, const QByteArray &cookie,
			OscarAccount *account, QObject *parent=0, const char *name=0, bool=false);

		~OscarSocket();

		/**
		 * A reimplementation of QHostAddress::setAddress to convert
		 * a QString to an ipv4 address for certain functions that
		 * required QHostAddress::setAddress in the past
		 */
		DWORD setIPv4Address(const QString &address);

		bool isICQ() { return mIsICQ; }

		/*
		 * Sends an authorization request to the server
		 */
		void sendLoginRequest();

		/*
		 * encodes a password using md5, outputs to digest [AIM]
		 */
		void encodePassword(char *digest);
		/*
		 * same as above but for icq which needs a XOR method to encode the password
		 *  returns the encoded password  [ICQ]
		 */
		void encodePasswordXOR(const QString &originalPassword, QString &encodedPassword);

		/*
		 * Logs in the user!  [OSCAR]
		 *
		 * @param host				Login server.
		 * @param port				Login port.
		 * @param name				Screen name/UIN
		 * @param password 			password
		 * @param profile			UserProfile for AIM connection
		 * @param initialStatus		Login Status (Online, Away, etc)
		 */
		void doLogin(
			const QString &host,
			int port,
			const QString &name,
			const QString &password,
			const QString &userProfile,
			const unsigned long initialStatus,
			const QString &awayMessage);

		/*
		 * Gets the rate info from the server
		 */
		void sendRateInfoRequest();
		/*
		 * requests the current user's info [AIM]
		 */
		void requestMyUserInfo();
		/*
		 * Sends idle time  [OSCAR]
		 */
		void sendIdleTime(DWORD time);
		/*
		 * requests ssi data from the server  [OSCAR]
		 */
		void sendBuddyListRequest();
		/*
		 * Sends message to dest
		 */
		void sendIM(
			const QString &message,
			OscarContact *contact,
			bool isAuto);

		/** Requests sn's user info */
		void sendUserProfileRequest(const QString &sn);

		/** Sends someone a warning */
		void sendWarning(const QString &target, bool isAnonymous);

		/** Changes a user's password (AIM Method) */
		void sendChangePassword(const QString &newpw, const QString &oldpw);

		/** Joins the given chat room */
		void sendChatJoin(const QString &name, const int exchange);

		/** Sends a request for direct IM */
		void sendDirectIMRequest(const QString &sn);

		/** Sends a direct IM denial */
		void sendDirectIMDeny(const QString &sn);

		/** Sends a direct IM accept */
		void sendDirectIMAccept(const QString &sn);

		/*
		 * Sends our capabilities to the server
		 * for AIM this also sends the userprofile
		 * @param profile AIM UserProfile or QString:null if no profile to send
		 * @param caps supported capabilities or 0 if you want default caps to be sent
		 */
		void sendLocationInfo(const QString &profile, const unsigned long caps=0);

		/*
		 * Signs ourselves off
		 */
		virtual void doLogoff();

		/*
		 * Adds a buddy to the server side buddy list
		 */
		virtual void sendAddBuddy(const QString &name, const QString &group);

		/*
		 * Changes a buddy's group on the server
		 */
		virtual void sendChangeBuddyGroup(const QString &buddyName,
			const QString &oldGroup, const QString &newGroup);

		/*
		 * changes the visibility setting to @value
		 */
		void sendChangeVisibility(BYTE value);

		/*
		 * Changes a contacts alias on the serverside contactlist
		 * @p budName real contact name
		 * @p budGroup name of group this contact is in
		 * @p newAlias alias to set for this contact
		 */
		void sendRenameBuddy(const QString &budName,
			const QString &budGroup, const QString &newAlias);

		/*
		 * Adds a group to the server side buddy list
		 */
		virtual void sendAddGroup(const QString &name);

		/*
		 * Changes a group's name on the server side buddy list
		 */
		virtual void sendChangeGroupName(const QString &currentName,
			const QString &newName);

		/*
		 * Removes a group from the server side information
		 */
		virtual void sendDelGroup(const QString &groupName);

		/*
		 * Deletes a buddy from the server side buddy list
		 */
		virtual void sendDelBuddy(const QString &budName, const QString &budGroup);

		/*
		 * Sends the server lots of information
		 * about the currently logged in user
		 */
		void sendInfo();

		/*
		 * sends a status change, status is one of OSCAR_
		 * awayMessage is only used for AIM currently
		 */
//		void sendStatus(const unsigned int status, const QString &awayMessage = QString::null);

		/*
		 * Sets the away message for AIM, makes user away
		 */
		void sendAIMAway(bool away, const QString &message=0L);

		/*
		 * send status, i.e. AWAY, NA, OCC (ICQ method)
		 */
		void sendICQStatus(unsigned long status);

		/*
		 * Adds a direct connection info TLV to Buffer directInfo
		 */
		void fillDirectInfo(Buffer &directInfo);

		/** Blocks user sname */
		void sendBlock(const QString &sname);

		/** Removes the block on user sname */
		void sendRemoveBlock(const QString &sname);

		/*
		 * Sends a typing notification to the server
		 * @param screenName The name of the person to send to
		 * @param notifyType Type of notify to send
		 */
		void sendMiniTypingNotify(const QString &screenName, TypingNotify notifyType);
#if 0
		/** Initiate a transfer of the given file to the given sn */
		void sendFileSendRequest(const QString &sn, const KFileItem &finfo);
		/** Sends a file transfer deny to @sn */
		void sendFileSendDeny(const QString &sn);
		/** Accepts a file transfer from sn, returns OscarConnection created */
		OscarConnection *sendFileSendAccept(const QString &sn, const QString &fileName);
#endif

		/** send request for offline messages (ICQ method) */
		void sendReqOfflineMessages();
		/** send acknowledgment for offline messages (ICQ method) */
		void sendAckOfflineMessages();
		/** sends a KEEPALIVE packet, empty FLAP channel 5 */
		void sendKeepalive();

		/*
		* start a contact search by providing an UIN, ICQ SPECIFIC
		*/
		void sendCLI_SEARCHBYUIN(const unsigned long uin);

		/*
		 * same but more evil than you can imagine ;)
		 */
		void sendCLI_SEARCHWP(
			const QString &first,
			const QString &last,
			const QString &nick,
			const QString &mail,
			int minage,
			int maxage,
			int sex,
			int lang, // TODO: unused
			const QString &city,
			const QString state,
			int country,
			const QString &company,
			const QString &department,
			const QString &position,
			int occupation,
			bool onlineOnly); /*...*/

		/*
		 * Starts a userinfo request for ICQ, returns the sequence sent out with the request
		 * Use it to compare a server reply's sequence
		 */
		WORD sendReqInfo(const unsigned long uin);

		/*
		 * sends the general info for the uin owner to icq
		 */
		void sendCLI_METASETGENERAL(const ICQGeneralUserInfo &i);

		/*
		 * sends the work info for the uin owner to icq
		 */
		void sendCLI_METASETWORK(const ICQWorkUserInfo &i);


		void sendCLI_METASETSECURITY(bool requireauth, bool webaware, BYTE direct);

		void sendAuthRequest(const QString &contact, const QString &reason);
		void sendAuthReply(const QString &contact, const QString &reason, bool grant);

	public slots:
		/*
		 * This is called when a connection is established
		 */
		void slotConnected();

	private:
	/** adds the flap version to the buffer */
	void putFlapVer(Buffer &buf);

	/** Reads a FLAP header from the input */
	FLAP getFLAP();

	/** Sends the output buffer, and clears it */
	void sendBuf(Buffer &buf, BYTE chan);

	/**
	* Sends login information, actually logs
	* onto the server
	*/
	void sendLoginAIM();
	void sendLoginICQ();

	/*
	 * Called when a cookie is received
	 */
	void connectToBos();
	/*
	 * Sends the authorization cookie to the BOS server
	 */
	void sendCookie();
	/*
	 * Parses the rate info response
	 */
	void parseRateInfoResponse(Buffer &inbuf);
	/*
	* Tells the server we accept it's communist rate
	* limits, even though I have no idea what they mean
	*/
	void sendRateAck();
	/*
	 * Sends privacy flags to the server
	 */
	void sendPrivacyFlags();
	/*
	 * parse my user info
	 */
	void parseMyUserInfo(Buffer &inbuf);
	/*
	 * finds a tlv of type @p typ in the list
	 */
	TLV *findTLV(QPtrList<TLV> &l, WORD typ);
	/*
	* Parse the server's authorization response
	* (which hopefully contains the cookie)
	*/
	void parseAuthResponse(Buffer &inbuf);
	/*
	 * The program does this when a key is received
	 */
	void parsePasswordKey(Buffer &inbuf);
	/*
	 * tells the server that the client is
	 * ready to receive commands & stuff
	 */
	void sendClientReady();
	/*
	 * Sends versions so that we get proper rate info
	 */
	void sendVersions(const WORD *families, const int len);

	/*
	* Handles AOL's evil attempt to thwart 3rd
	* party apps using Oscar.  It requests a
	* segment and offset of aim.exe.  We can
	* thwart it with help from the good people
	* at Gaim
	*/
	void parseMemRequest(Buffer &inbuf);

	/*
	 * parses incoming contactlist (roster) data
	 */
	void parseRosterData(Buffer &inbuf);

	/*
	 * parses incoming ack for current contactlist timestamp/length
	 * @see sendBuddyListRequest() for data sent on CLI_CHECKROSTER
	 */
	void parseRosterOk(Buffer &inbuf);

	/*
	 * Requests the user's SSI rights
	 */
	void requestBOSRights();
	/*
	 * Parses SSI rights data
	 */
	void parseBOSRights(Buffer &inbuf);
	/*
	 * Parses the server ready response
	 */
	void parseServerReady(Buffer &inbuf);
	/*
	 * parses server version info
	 */
	void parseServerVersions(Buffer &inbuf);

	/** Parses Message of the day */
	void parseMessageOfTheDay(Buffer &inbuf);

	/** Requests location rights */
	void requestLocateRights();

	/** Requests a bunch of information (permissions, rights, my user info, etc) from server */
	void requestInfo();

	/** adds a mask of the groups that you want to be able to see you to the buffer */
	void sendGroupPermissionMask();

	/** adds a request for buddy list rights to the buffer */
	void requestBuddyRights();

	/** adds a request for msg rights to the buffer */
	void requestMsgRights();

	/** Parses the locate rights provided by the server */
	void parseLocateRights(Buffer &inbuf);

	/** Parses buddy list rights from the server */
	void parseBuddyRights(Buffer &inbuf);

	/** Parses msg rights info from server */
	void parseMsgRights(Buffer &inbuf);

	/*
	 * Parses an incoming IM
	 */
	void parseIM(Buffer &inbuf);

	/*
	 * parses a type-1 message
	 * called by parseIM
	 */
	void parseSimpleIM(Buffer &inbuf, const UserInfo &u);
	/*
	 * parses a type-4 message
	 * called by parseIM
	 */
	void parseServerIM(Buffer &inbuf, const UserInfo &u);

	/** parses the aim standard user info block */
	//UserInfo parseUserInfo(Buffer &inbuf);
	bool parseUserInfo(Buffer &inbuf, UserInfo &u);

	/*
	 * parses a capabilities block contained in inbuf
	 * inbuf should NOT contain anything else or it'll break ya neck ;)
	 */
	const DWORD parseCapabilities(Buffer &inbuf);

	/** Activates the SSI list on the server */
	void sendSSIActivate();

	/** Parses the oncoming buddy server notification */
	void parseUserOnline(Buffer &);

	/** Parses offgoing buddy message from server */
	void parseUserOffline(Buffer &);

	/** Parses someone's user info */
	void parseUserProfile(Buffer &);
	/*
	 * Handles a redirect
	 * TODO: unused
	 */
//	void parseRedirect(Buffer &);

	/** Parses a message ack from the server */
	void parseMsgAck(Buffer &);

	/** Parses a minityping notification from server */
	void parseMiniTypeNotify(Buffer &);

	void parseSRV_FROMICQSRV(Buffer &);
	void parseAdvanceMessage(Buffer &, UserInfo &);

	/** Parses a rate change */
	void parseRateChange(Buffer &inbuf);

	/** Sends SSI add, modify, or delete request to reuse code */
	void sendSSIAddModDel(SSI *item, WORD request_type);

	/** Parses the SSI acknowledgment */
	void parseSSIAck(Buffer &inbuf);

	/** Parses a warning notification */
	void parseWarningNotify(Buffer &inbuf);

	/** Parses a message sending error */
	void parseError(WORD family, Buffer &inbuf);

	/** Parses a missed message notification */
	void parseMissedMessage(Buffer &inbuf);

	/** Request, deny, or accept a rendezvous session with someone
	type == 0: request
	type == 1: deny
	type == 2: accept  */
	void sendRendezvous(const QString &sn, WORD type, DWORD rendezvousType, const KFileItem *finfo=0L);

	/** Sends a 0x0013,0x0002 (requests SSI rights information) */
	void sendSSIRightsRequest();

	/** Sends a 0x0013,0x0004 (requests SSI data?) */
	void sendSSIRequest();

	/*
	 * Parses a SNAC(0x0013,0x0003) (SSI rights) from the server
	 */
	void parseSSIRights(Buffer &inbuf);

	/*
	 * Sends parameters for ICBM messages
	 */
	void sendMsgParams();

	/*
	 * Returns the appropriate server socket, based on the capability flag it is passed.
	 */
#if 0
	OncomingSocket * serverSocket(DWORD capflag);
#endif

	// parse DISCONNECT messages on channel 4, ICQ specific
	void parseConnectionClosed(Buffer &inbuf);

	/*
	 * send a CLI_TOICQSRV with subcommand and DATA supplied in data
	 * returns the sequence sent out with the packet
	 * incoming server replies will have the same sequence!
	 */
	WORD sendCLI_TOICQSRV(const WORD subcommand, Buffer &data);

	void startKeepalive();
	void stopKeepalive();

	void parseAuthReply(Buffer &inbuf);

	void sendBuddylistAdd(QStringList &contacts);

	private slots:
	/** Called when a connection has been closed */
	void slotConnectionClosed();
	/** Called when the server aknowledges the connection */
	void OnConnAckReceived();
	/** called when a conn ack is received for the BOS connection */
	void OnBosConnAckReceived();
	/** Called when the server is ready for normal commands */
	//void OnServerReady();
	/** Called on connection to bos server */
	void OnBosConnect();
	/** Called when a direct IM is received */
	void OnDirectIMReceived(QString, QString, bool);
	/** Called when a direct IM connection suffers an error */
	void OnDirectIMError(QString, int);
	/** Called when a direct IM connection bites the dust */
	void OnDirectIMConnectionClosed(QString);
	/** Called whenever a direct IM connection gets a typing notification */
	void OnDirectMiniTypeNotification(QString screenName, int notify);
	/** Called when a direct connection is set up and ready for use */
	void OnDirectIMReady(QString name);
	/** Called when a file transfer begins */
/*
	void OnFileTransferBegun(OscarConnection *con, const QString& file,
		const unsigned long size, const QString &recipient);
*/
	void slotKeepaliveTimer();

	signals:

	/**
	 * Emitted when there is more information to read from the socket
	 * This is only used in slotRead() since I haven't found a way to
	 * emit socket()->readyRead directly yet. (Matt)
	 */
	 void moreToRead();

	/*
	 * emitted when any kind of Instant Message was received
	 * @p type describes the message type, i.e. normal-msg, away-msg, sms-msg ...
	 * @p message contains the message as received
	 * @p user contains the screenname/UIN of the sender
	 */
	void gotIM(OscarSocket::OscarMessageType type, QString &message, const QString &user);

	/*
	 * The server has sent the key with which to encrypt the password
	 */
	void keyReceived();
	/*
	 * The bos server is ready to be sent commands
	 */
	//void serverReady();
	/*
	 * A contact went offline
	 */
	void gotOffgoingBuddy(QString);
	/*
	 * A contact changed his status to something else then offline
	 */
	void gotBuddyChange(const UserInfo &);
	/*
	 * A user profile was received
	 */
	void gotUserProfile(const UserInfo &, const QString &profile, const QString &away);
	/*
	 * Emitted when the status of the connection changes during login
	 */
//	void connectionChanged(int, QString);
	/*
	 * Emitted when my user info is received
	 */
	void gotMyUserInfo(const UserInfo &);
	/*
	 * A buddy list has been received
	 */
	void gotConfig(AIMBuddyList &);
	/*
	 * emitted when we have received an ack from the server
	 */
	void gotAck(QString, int);

	/*
	 * Emitted when our status has changed, internalStatus is one of OSCAR_*
	 */
	void statusChanged(const unsigned int internalStatus);

	/** Emitted when the logged in user has been warned
	 * The int is the new warning level.
	 * The QString is the name of the user which warned us (QString::null if anonymous)
	 * WARNING: this is emitted every time the server notifies us about our warning level,
	 * so natural decreases in level will be signalled.
	 */
	void gotWarning(int, QString);
	/** Emitted when someone has requested a direct IM session with us */
	void gotDirectIMRequest(QString);
	/** Emitted when someone has requested to send a file to us */
	void gotFileSendRequest(QString, QString, QString, unsigned long);

	/*
	 * emitted when a usersearch yielded a result, ICQ SPECIFIC
	 */
	void gotSearchResult(ICQSearchResult &, const int);

	/*
	 * emitted when a userinfo request yielded a result, ICQ SPECIFIC
	 * first argument = sequence of the server reply
	 */
	void gotICQGeneralUserInfo(const int, const ICQGeneralUserInfo &);
	void gotICQWorkUserInfo(const int, const ICQWorkUserInfo &);
	void gotICQMoreUserInfo(const int, const ICQMoreUserInfo &);
	void gotICQAboutUserInfo(const int, const QString &);
	void gotICQEmailUserInfo(const int, const ICQMailList &);
	void gotICQInfoItemList(const int, const ICQInfoItemList &);
	void gotICQInfoItemList(const int, const ICQInfoItemList &, const ICQInfoItemList &);

	/*
	 * emitted after CLIENT_READY packet, the account can now
	 * do whatever it likes to do after successful login.
	 */
	void loggedIn();

	/*
	 * emitted when we received an authorization reply
	 */
	void gotAuthReply(const QString &, const QString &, bool);

	protected slots:
		/*
		 * This function is called when there is data to be read from the socket
		 */
		virtual void slotRead();
		void slotDelayConnectingPhaseTimeout();

	protected:
		ICQInfoItemList extractICQItemList( Buffer& theBuffer );

	private:
		/*
		 * The OscarAccount we're assocated with
		 */
		OscarAccount *mAccount;

		/*
		 * The key used to encrypt the password
		 */
		char * key;
		/*
		 * The user's password
		 */
		QString loginPassword;
		 /*
		  * AIM profile sent at login
		  */
		QString loginProfile;
		 /*
		  * status sent at login, contents depend on AIM or ICQ being used
		  */
		unsigned long loginStatus;

		// The authorization cookie
		char * mCookie;
		// The length of the cookie
		WORD mCookieLength;
		// ip address of the bos server
		QString bosServer;
		// The port of the bos server
		int bosPort;
		/** Stores rate class information */
		QPtrList<RateClass> rateClasses;
		/** tells whether we are idle */
		bool idle;
		/** Socket for direct connections */
		OncomingSocket *mDirectIMMgr;
		/** Socket for file transfers */
		OncomingSocket *mFileTransferMgr;
		/** SSI server stored data */
		SSIData ssiData;
		/** Socket for direct connections */
		KExtendedSocket * connsock;
		// Tells if we are connected to the server and ready to operate
		bool isLoggedIn;

		/*
		 * counter to find out if we got all packets needed before sending
		 * out more info and the final CLI_READY command which is the end
		 * of a login procedure.
		 */
		int gotAllRights;

		int keepaliveTime;
		QTimer *keepaliveTimer;

		bool mIsICQ;
		/*
		 * one up sequence used for packets of type CLI_TOICQSRV
		 */
		WORD toicqsrv_seq;
		/*
		 * sequence number in a FLAP header
		 * incremented after every command sent to the oscar server
		 */
		WORD flapSequenceNum;

		DWORD mDirectConnnectionCookie;

		enum FirstPresenceBlock
		{
			Waiting=0, GotSome, GotAll
		};
		FirstPresenceBlock awaitingFirstPresenceBlock;

	signals:
		// Called when an SSI acknowledgment is received
		// FIXME: What was this for, nothing connected to this signal [mETz]
		//void SSIAck();
		// emitted when BOS rights are received
//		void gotBOSRights(WORD,WORD);
		// emitted when a buddy gets blocked
		void denyAdded(QString);
		// emitted when a block is removed on a buddy
		void denyRemoved(QString);
		// Tells when the connection ack has been received on channel 1
		void connAckReceived();
		// emitted when a direct connection has been terminated
		void directIMConnectionClosed(QString name);
};
#endif
// vim: set noet ts=4 sts=4 sw=4:
