/*
 * Copyright (C) 2009  Barracuda Networks, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include "stunbinding.h"

#include <QHostAddress>
#include "stunmessage.h"
#include "stuntypes.h"
#include "stuntransaction.h"

namespace XMPP {

class StunBinding::Private : public QObject
{
	Q_OBJECT

public:
	StunBinding *q;
	StunTransactionPool *pool;
	StunTransaction *trans;
	QHostAddress addr;
	int port;
	QString errorString;
	bool use_extPriority, use_extIceControlling, use_extIceControlled;
	quint32 extPriority;
	bool extUseCandidate;
	quint64 extIceControlling, extIceControlled;

	Private(StunBinding *_q) :
		QObject(_q),
		q(_q),
		pool(0),
		trans(0),
		use_extPriority(false),
		use_extIceControlling(false),
		use_extIceControlled(false),
		extUseCandidate(false)
	{
	}

	~Private()
	{
		if(trans)
			pool->remove(trans);
	}

	void start()
	{
		Q_ASSERT(!trans);

		trans = new StunTransaction(this);
		connect(trans, SIGNAL(finished(const XMPP::StunMessage &)), SLOT(trans_finished(const XMPP::StunMessage &)));
		connect(trans, SIGNAL(error(XMPP::StunTransaction::Error)), SLOT(trans_error(XMPP::StunTransaction::Error)));

		StunMessage message;
		message.setClass(StunMessage::Request);
		message.setMethod(StunTypes::Binding);
		QByteArray id = pool->generateId();
		message.setId((const quint8 *)id.data());

		QList<StunMessage::Attribute> list;

		if(use_extPriority)
		{
			StunMessage::Attribute a;
			a.type = StunTypes::PRIORITY;
			a.value = StunTypes::createPriority(extPriority);
			list += a;
		}

		if(extUseCandidate)
		{
			StunMessage::Attribute a;
			a.type = StunTypes::USE_CANDIDATE;
			list += a;
		}

		if(use_extIceControlling)
		{
			StunMessage::Attribute a;
			a.type = StunTypes::ICE_CONTROLLING;
			a.value = StunTypes::createIceControlling(extIceControlling);
			list += a;
		}

		if(use_extIceControlled)
		{
			StunMessage::Attribute a;
			a.type = StunTypes::ICE_CONTROLLED;
			a.value = StunTypes::createIceControlled(extIceControlled);
			list += a;
		}

		message.setAttributes(list);

		trans->start(pool->mode(), message, pool->username(), pool->password());

		pool->insert(trans);
	}

private slots:
	void trans_finished(const XMPP::StunMessage &response)
	{
		pool->remove(trans);
		trans = 0;

		if(response.mclass() == StunMessage::ErrorResponse)
		{
			errorString = "Server responded with an error.";
			emit q->error(StunBinding::ErrorRejected);
			return;
		}

		QHostAddress saddr;
		quint16 sport = 0;

		QByteArray val;
		val = response.attribute(StunTypes::XOR_MAPPED_ADDRESS);
		if(!val.isNull())
		{
			if(!StunTypes::parseXorMappedAddress(val, response.magic(), response.id(), &saddr, &sport))
			{
				errorString = "Unable to parse XOR-MAPPED-ADDRESS response.";
				emit q->error(StunBinding::ErrorProtocol);
				return;
			}
		}
		else
		{
			val = response.attribute(StunTypes::MAPPED_ADDRESS);
			if(!val.isNull())
			{
				if(!StunTypes::parseMappedAddress(val, &saddr, &sport))
				{
					errorString = "Unable to parse MAPPED-ADDRESS response.";
					emit q->error(StunBinding::ErrorProtocol);
					return;
				}
			}
			else
			{
				errorString = "Response does not contain XOR-MAPPED-ADDRESS or MAPPED-ADDRESS.";
				emit q->error(StunBinding::ErrorProtocol);
				return;
			}
		}

		addr = saddr;
		port = sport;
		emit q->success();
	}

	void trans_error(XMPP::StunTransaction::Error e)
	{
		pool->remove(trans);
		trans = 0;

		if(e == StunTransaction::ErrorTimeout)
		{
			errorString = "Request timed out.";
			emit q->error(StunBinding::ErrorTimeout);
		}
		else
		{
			errorString = "Generic transaction error.";
			emit q->error(StunBinding::ErrorGeneric);
		}
	}
};

StunBinding::StunBinding(StunTransactionPool *pool) :
	QObject(pool)
{
	d = new Private(this);
	d->pool = pool;
}

StunBinding::~StunBinding()
{
	delete d;
}

void StunBinding::setPriority(quint32 i)
{
	d->use_extPriority = true;
	d->extPriority = i;
}

void StunBinding::setUseCandidate(bool enabled)
{
	d->extUseCandidate = enabled;
}

void StunBinding::setIceControlling(quint64 i)
{
	d->use_extIceControlling = true;
	d->extIceControlling = i;
}

void StunBinding::setIceControlled(quint64 i)
{
	d->use_extIceControlled = true;
	d->extIceControlled = i;
}

void StunBinding::start()
{
	d->start();
}

QHostAddress StunBinding::reflexiveAddress() const
{
	return d->addr;
}

int StunBinding::reflexivePort() const
{
	return d->port;
}

QString StunBinding::errorString() const
{
	return d->errorString;
}

}

#include "stunbinding.moc"
