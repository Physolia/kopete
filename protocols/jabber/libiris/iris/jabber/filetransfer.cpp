/*
 * filetransfer.cpp - File Transfer
 * Copyright (C) 2004  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include"filetransfer.h"

#include<qtimer.h>
#include<qptrlist.h>
#include<qguardedptr.h>
#include<qfileinfo.h>
#include"xmpp_xmlcommon.h"
#include"s5b.h"

#define SENDBUFSIZE 65536

using namespace XMPP;

// firstChildElement
//
// Get an element's first child element
static QDomElement firstChildElement(const QDomElement &e)
{
	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		if(n.isElement())
			return n.toElement();
	}
	return QDomElement();
}

//----------------------------------------------------------------------------
// FileTransfer
//----------------------------------------------------------------------------
class FileTransfer::Private
{
public:
	FileTransferManager *m;
	JT_FT *ft;
	Jid peer;
	QString fname;
	Q_LLONG size;
	Q_LLONG sent;
	bool rangeSupported;
	Q_LLONG rangeOffset, rangeLength;
	QString streamType;
	bool needStream;
	QString id, iq_id;
	S5BConnection *c;
	Jid proxy;
	int state;
	bool sender;
};

FileTransfer::FileTransfer(FileTransferManager *m, QObject *parent)
:QObject(parent)
{
	d = new Private;
	d->m = m;
	d->ft = 0;
	d->c = 0;
	reset();
}

FileTransfer::~FileTransfer()
{
	reset();
	delete d;
}

void FileTransfer::reset()
{
	d->m->unlink(this);

	delete d->ft;
	d->ft = 0;

	delete d->c;
	d->c = 0;

	d->state = Idle;
	d->needStream = false;
	d->sent = 0;
	d->sender = false;
}

void FileTransfer::setProxy(const Jid &proxy)
{
	d->proxy = proxy;
}

void FileTransfer::sendFile(const Jid &to, const QString &fname, Q_LLONG size)
{
	d->state = Requesting;
	d->peer = to;
	d->fname = fname;
	d->size = size;
	d->sender = true;
	d->id = d->m->link(this);

	d->ft = new JT_FT(d->m->client()->rootTask());
	connect(d->ft, SIGNAL(finished()), SLOT(ft_finished()));
	QStringList list;
	list += "http://jabber.org/protocol/bytestreams";
	d->ft->request(to, d->id, fname, size, list);
	d->ft->go(true);
}

int FileTransfer::dataSizeNeeded() const
{
	int pending = d->c->bytesToWrite();
	if(pending >= SENDBUFSIZE)
		return 0;
	Q_LLONG left = d->size - (d->sent + pending);
	int size = SENDBUFSIZE - pending;
	if((Q_LLONG)size > left)
		size = (int)left;
	return size;
}

void FileTransfer::writeFileData(const QByteArray &a)
{
	int pending = d->c->bytesToWrite();
	Q_LLONG left = d->size - (d->sent + pending);
	if(left == 0)
		return;

	QByteArray block;
	if((Q_LLONG)a.size() > left) {
		block = a.copy();
		block.resize((uint)left);
	}
	else
		block = a;
	d->c->write(block);
}

Jid FileTransfer::peer() const
{
	return d->peer;
}

QString FileTransfer::fileName() const
{
	return d->fname;
}

Q_LLONG FileTransfer::fileSize() const
{
	return d->size;
}

bool FileTransfer::rangeSupported() const
{
	return d->rangeSupported;
}

void FileTransfer::accept(Q_LLONG offset, Q_LLONG length)
{
	d->state = Connecting;
	d->rangeOffset = offset;
	d->rangeLength = length;
	d->streamType = "http://jabber.org/protocol/bytestreams";
	d->m->con_accept(this);
}

void FileTransfer::close()
{
	if(d->state == Idle)
		return;
	if(d->state == WaitingForAccept)
		d->m->con_reject(this);
	else if(d->state == Active)
		d->c->close();
	reset();
}

S5BConnection *FileTransfer::s5bConnection() const
{
	return d->c;
}

void FileTransfer::ft_finished()
{
	JT_FT *ft = d->ft;
	d->ft = 0;

	if(ft->success()) {
		d->state = Connecting;
		d->rangeOffset = ft->rangeOffset();
		d->rangeLength = ft->rangeLength();
		d->streamType = ft->streamType();
		d->c = d->m->client()->s5bManager()->createConnection();
		connect(d->c, SIGNAL(connected()), SLOT(s5b_connected()));
		connect(d->c, SIGNAL(connectionClosed()), SLOT(s5b_connectionClosed()));
		connect(d->c, SIGNAL(bytesWritten(int)), SLOT(s5b_bytesWritten(int)));
		connect(d->c, SIGNAL(error(int)), SLOT(s5b_error(int)));

		if(d->proxy.isValid())
			d->c->setProxy(d->proxy);
		d->c->connectToJid(d->peer, d->id);
		accepted();
	}
	else {
		reset();
		if(ft->statusCode() == 403)
			error(ErrReject);
		else
			error(ErrNeg);
	}
}

void FileTransfer::takeConnection(S5BConnection *c)
{
	d->c = c;
	connect(d->c, SIGNAL(connected()), SLOT(s5b_connected()));
	connect(d->c, SIGNAL(connectionClosed()), SLOT(s5b_connectionClosed()));
	connect(d->c, SIGNAL(readyRead()), SLOT(s5b_readyRead()));
	connect(d->c, SIGNAL(error(int)), SLOT(s5b_error(int)));
	if(d->proxy.isValid())
		d->c->setProxy(d->proxy);
	d->c->accept();
	accepted();
}

void FileTransfer::s5b_connected()
{
	d->state = Active;
	connected();
}

void FileTransfer::s5b_connectionClosed()
{
	reset();
	error(ErrStream);
}

void FileTransfer::s5b_readyRead()
{
	QByteArray a = d->c->read();
	Q_LLONG need = d->size - d->sent;
	if((Q_LLONG)a.size() > need)
		a.resize((uint)need);
	d->sent += a.size();
	if(d->sent == d->size)
		reset();
	readyRead(a);
}

void FileTransfer::s5b_bytesWritten(int x)
{
	d->sent += x;
	if(d->sent == d->size)
		reset();
	bytesWritten(x);
}

void FileTransfer::s5b_error(int x)
{
	reset();
	if(x == S5BConnection::ErrRefused || x == S5BConnection::ErrConnect)
		error(ErrConnect);
	else
		error(ErrStream);
}

void FileTransfer::man_waitForAccept(const FTRequest &req)
{
	d->state = WaitingForAccept;
	d->peer = req.from;
	d->id = req.id;
	d->iq_id = req.iq_id;
	d->fname = req.fname;
	d->size = req.size;
	d->rangeSupported = req.rangeSupported;
}

//----------------------------------------------------------------------------
// FileTransferManager
//----------------------------------------------------------------------------
class FileTransferManager::Private
{
public:
	Client *client;
	QPtrList<FileTransfer> list, incoming;
	JT_PushFT *pft;
};

FileTransferManager::FileTransferManager(Client *client)
:QObject(client)
{
	d = new Private;
	d->client = client;

	d->pft = new JT_PushFT(d->client->rootTask());
	connect(d->pft, SIGNAL(incoming(const FTRequest &)), SLOT(pft_incoming(const FTRequest &)));
}

FileTransferManager::~FileTransferManager()
{
	d->incoming.setAutoDelete(true);
	d->incoming.clear();
	delete d->pft;
	delete d;
}

Client *FileTransferManager::client() const
{
	return d->client;
}

FileTransfer *FileTransferManager::createTransfer()
{
	FileTransfer *ft = new FileTransfer(this);
	return ft;
}

FileTransfer *FileTransferManager::takeIncoming()
{
	if(d->incoming.isEmpty())
		return 0;

	FileTransfer *ft = d->incoming.getFirst();
	d->incoming.removeRef(ft);

	// move to active list
	d->list.append(ft);
	return ft;
}

void FileTransferManager::pft_incoming(const FTRequest &req)
{
	bool found = false;
	for(QStringList::ConstIterator it = req.streamTypes.begin(); it != req.streamTypes.end(); ++it) {
		if((*it) == "http://jabber.org/protocol/bytestreams") {
			found = true;
			break;
		}
	}
	if(!found) {
		d->pft->respondError(req.from, req.iq_id, 400, "No valid stream types");
		return;
	}
	if(!d->client->s5bManager()->isAcceptableSID(req.from, req.id)) {
		d->pft->respondError(req.from, req.iq_id, 400, "SID in use");
		return;
	}

	FileTransfer *ft = new FileTransfer(this);
	ft->man_waitForAccept(req);
	d->incoming.append(ft);
	incomingReady();
}

void FileTransferManager::s5b_incomingReady(S5BConnection *c)
{
	QPtrListIterator<FileTransfer> it(d->list);
	FileTransfer *ft = 0;
	for(FileTransfer *i; (i = it.current()); ++it) {
		if(i->d->needStream && i->d->peer.compare(c->peer()) && i->d->id == c->sid()) {
			ft = i;
			break;
		}
	}
	if(!ft) {
		c->close();
		delete c;
		return;
	}
	ft->takeConnection(c);
}

QString FileTransferManager::link(FileTransfer *ft)
{
	d->list.append(ft);
	return d->client->s5bManager()->genUniqueSID(ft->d->peer);
}

void FileTransferManager::con_accept(FileTransfer *ft)
{
	ft->d->needStream = true;
	d->pft->respondSuccess(ft->d->peer, ft->d->iq_id, ft->d->rangeOffset, ft->d->rangeLength, ft->d->streamType);
}

void FileTransferManager::con_reject(FileTransfer *ft)
{
	d->pft->respondError(ft->d->peer, ft->d->iq_id, 403, "Declined");
}

void FileTransferManager::unlink(FileTransfer *ft)
{
	d->list.removeRef(ft);
}

//----------------------------------------------------------------------------
// JT_FT
//----------------------------------------------------------------------------
class JT_FT::Private
{
public:
	QDomElement iq;
	Jid to;
	Q_LLONG rangeOffset, rangeLength;
	QString streamType;
	QStringList streamTypes;
};

JT_FT::JT_FT(Task *parent)
:Task(parent)
{
	d = new Private;
}

JT_FT::~JT_FT()
{
	delete d;
}

void JT_FT::request(const Jid &to, const QString &_id, const QString &fname, Q_LLONG size, const QStringList &streamTypes)
{
	QDomElement iq;
	d->to = to;
	iq = createIQ(doc(), "set", to.full(), id());
	QDomElement si = doc()->createElement("si");
	si.setAttribute("xmlns", "http://jabber.org/protocol/si");
	si.setAttribute("id", _id);
	si.setAttribute("profile", "http://jabber.org/protocol/si/profile/file-transfer");

	QDomElement file = doc()->createElement("file");
	file.setAttribute("xmlns", "http://jabber.org/protocol/si/profile/file-transfer");
	file.setAttribute("name", fname);
	file.setAttribute("size", QString::number(size));
	QDomElement range = doc()->createElement("range");
	file.appendChild(range);
	si.appendChild(file);

	QDomElement feature = doc()->createElement("feature");
	feature.setAttribute("xmlns", "http://jabber.org/protocol/feature-neg");
	QDomElement x = doc()->createElement("x");
	x.setAttribute("xmlns", "jabber:x:data");
	x.setAttribute("type", "form");

	QDomElement field = doc()->createElement("field");
	field.setAttribute("var", "stream-method");
	field.setAttribute("type", "list-single");
	for(QStringList::ConstIterator it = streamTypes.begin(); it != streamTypes.end(); ++it) {
		QDomElement option = doc()->createElement("option");
		QDomElement value = doc()->createElement("value");
		value.appendChild(doc()->createTextNode(*it));
		option.appendChild(value);
		field.appendChild(option);
	}

	x.appendChild(field);
	feature.appendChild(x);

	si.appendChild(feature);
	iq.appendChild(si);

	d->streamTypes = streamTypes;
	d->iq = iq;
}

Q_LLONG JT_FT::rangeOffset() const
{
	return d->rangeOffset;
}

Q_LLONG JT_FT::rangeLength() const
{
	return d->rangeLength;
}

QString JT_FT::streamType() const
{
	return d->streamType;
}

void JT_FT::onGo()
{
	send(d->iq);
}

bool JT_FT::take(const QDomElement &x)
{
	if(!iqVerify(x, d->to, id()))
		return false;

	if(x.attribute("type") == "result") {
		QDomElement si = firstChildElement(x);
		if(si.attribute("xmlns") != "http://jabber.org/protocol/si" || si.tagName() != "si")
			return true; // ignore

		QString id = si.attribute("id");

		Q_LLONG range_offset = 0;
		Q_LLONG range_length = 0;
		QDomElement file = si.elementsByTagName("file").item(0).toElement();
		if(!file.isNull()) {
			QDomElement range = file.elementsByTagName("range").item(0).toElement();
			if(!range.isNull()) {
				int x;
				bool ok;
				x = range.attribute("offset").toLongLong(&ok);
				if(!ok || x < 0)
					return true;
				range_offset = x;
				x = range.attribute("length").toLongLong(&ok);
				if(!ok || x < 0)
					return true;
				range_length = x;
			}
		}

		QString streamtype;
		QDomElement feature = si.elementsByTagName("feature").item(0).toElement();
		if(!feature.isNull() && feature.attribute("xmlns") == "http://jabber.org/protocol/feature-neg") {
			QDomElement x = feature.elementsByTagName("x").item(0).toElement();
			if(!x.isNull() && x.attribute("type") == "submit") {
				QDomElement field = x.elementsByTagName("field").item(0).toElement();
				if(!field.isNull() && field.attribute("var") == "stream-method") {
					QDomElement value = field.elementsByTagName("value").item(0).toElement();
					if(!value.isNull())
						streamtype = value.text();
				}
			}
		}

		// must be one of the offered streamtypes
		bool found = false;
		for(QStringList::ConstIterator it = d->streamTypes.begin(); it != d->streamTypes.end(); ++it) {
			if((*it) == streamtype) {
				found = true;
				break;
			}
		}
		if(!found)
			return true;

		d->rangeOffset = range_offset;
		d->rangeLength = range_length;
		d->streamType = streamtype;
		setSuccess();
	}
	else {
		setError(x);
	}

	return true;
}

//----------------------------------------------------------------------------
// JT_PushFT
//----------------------------------------------------------------------------
JT_PushFT::JT_PushFT(Task *parent)
:Task(parent)
{
}

JT_PushFT::~JT_PushFT()
{
}

void JT_PushFT::respondSuccess(const Jid &to, const QString &id, Q_LLONG rangeOffset, Q_LLONG rangeLength, const QString &streamType)
{
	QDomElement iq = createIQ(doc(), "result", to.full(), id);
	QDomElement si = doc()->createElement("si");
	si.setAttribute("xmlns", "http://jabber.org/protocol/si");

	if(rangeOffset != 0 || rangeLength != 0) {
		QDomElement file = doc()->createElement("file");
		file.setAttribute("xmlns", "http://jabber.org/protocol/si/profile/file-transfer");
		QDomElement range = doc()->createElement("range");
		if(rangeOffset > 0)
			range.setAttribute("offset", QString::number(rangeOffset));
		if(rangeLength > 0)
			range.setAttribute("length", QString::number(rangeLength));
		file.appendChild(range);
		si.appendChild(file);
	}

	QDomElement feature = doc()->createElement("feature");
	feature.setAttribute("xmlns", "http://jabber.org/protocol/feature-neg");
	QDomElement x = doc()->createElement("x");
	x.setAttribute("xmlns", "jabber:x:data");
	x.setAttribute("type", "submit");

	QDomElement field = doc()->createElement("field");
	field.setAttribute("var", "stream-method");
	QDomElement value = doc()->createElement("value");
	value.appendChild(doc()->createTextNode(streamType));
	field.appendChild(value);

	x.appendChild(field);
	feature.appendChild(x);

	si.appendChild(feature);
	iq.appendChild(si);
	send(iq);
}

void JT_PushFT::respondError(const Jid &to, const QString &id, int code, const QString &str)
{
	QDomElement iq = createIQ(doc(), "error", to.full(), id);
	QDomElement err = textTag(doc(), "error", str);
	err.setAttribute("code", QString::number(code));
	iq.appendChild(err);
	send(iq);
}

bool JT_PushFT::take(const QDomElement &e)
{
	// must be an iq-set tag
	if(e.tagName() != "iq")
		return false;
	if(e.attribute("type") != "set")
		return false;

	QDomElement si = firstChildElement(e);
	if(si.attribute("xmlns") != "http://jabber.org/protocol/si" || si.tagName() != "si")
		return false;
	if(si.attribute("profile") != "http://jabber.org/protocol/si/profile/file-transfer")
		return false;

	Jid from(e.attribute("from"));
	QString id = si.attribute("id");

	QDomElement file = si.elementsByTagName("file").item(0).toElement();
	if(file.isNull())
		return true;

	QString fname = file.attribute("name");
	if(fname.isEmpty()) {
		respondError(from, id, 400, "Bad file name");
		return true;
	}

	// ensure kosher
	{
		QFileInfo fi(fname);
		fname = fi.fileName();
	}

	bool ok;
	Q_LLONG size = file.attribute("size").toLongLong(&ok);
	if(!ok || size < 1) {
		respondError(from, id, 400, "Bad file size");
		return true;
	}

	bool rangeSupported = false;
	QDomElement range = file.elementsByTagName("range").item(0).toElement();
	if(!range.isNull())
		rangeSupported = true;

	QStringList streamTypes;
	QDomElement feature = si.elementsByTagName("feature").item(0).toElement();
	if(!feature.isNull() && feature.attribute("xmlns") == "http://jabber.org/protocol/feature-neg") {
		QDomElement x = feature.elementsByTagName("x").item(0).toElement();
		if(!x.isNull() /*&& x.attribute("type") == "form"*/) {
			QDomElement field = x.elementsByTagName("field").item(0).toElement();
			if(!field.isNull() && field.attribute("var") == "stream-method" && field.attribute("type") == "list-single") {
				QDomNodeList nl = field.elementsByTagName("option");
				for(uint n = 0; n < nl.count(); ++n) {
					QDomElement e = nl.item(n).toElement();
					QDomElement value = e.elementsByTagName("value").item(0).toElement();
					if(!value.isNull())
						streamTypes += value.text();
				}
			}
		}
	}

	FTRequest r;
	r.from = from;
	r.iq_id = e.attribute("id");
	r.id = id;
	r.fname = fname;
	r.size = size;
	r.rangeSupported = rangeSupported;
	r.streamTypes = streamTypes;

	incoming(r);
	return true;
}
