/***************************************************************************
 *   Copyright © 2012 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "transaction.h"

// Qt includes
#include <QDebug>

// Own includes
#include "globals.h"
#include "transactiondbus.h"

namespace QApt {

class TransactionPrivate
{
    public:
        TransactionPrivate(const QString &id)
            : tid(id)
        {
            dbus = new OrgKubuntuQaptworkerTransactionInterface(QLatin1String("org.kubuntu.qaptworker"),
                                                                       tid, QDBusConnection::systemBus(),
                                                                       0);
        }

        TransactionPrivate(const TransactionPrivate &other)
            : dbus(other.dbus)
            , tid(other.tid)
            , uid(0)
        {
        }

        ~TransactionPrivate()
        {
            delete dbus;
        }

        // DBus
        OrgKubuntuQaptworkerTransactionInterface *dbus;

        // Data
        QString tid;
        int uid;
};

Transaction::Transaction(const QString &tid)
    : QObject()
    , d(new TransactionPrivate(tid))
{
    // Fetch property data from D-Bus
    sync();

    connect(d->dbus, SIGNAL(propertyChanged(int,QDBusVariant)),
            this, SLOT(updateProperty(int,QDBusVariant)));
}

Transaction::Transaction(const Transaction &other)
{
    d = other.d;
}

Transaction::~Transaction()
{
}

Transaction &Transaction::operator=(const Transaction& rhs)
{
    // Protect against self-assignment
    if (this == &rhs) {
        return *this;
    }
    d = rhs.d;
    return *this;
}

QString Transaction::transactionId() const
{
    return d->tid;
}

int Transaction::userId() const
{
    return d->uid;
}

void Transaction::setUserId(int uid)
{
    d->uid = uid;
}

void Transaction::sync()
{
    QDBusMessage call = QDBusMessage::createMethodCall(d->dbus->service(), d->tid,
                                                       "org.freedesktop.DBus.Properties", "GetAll");
    call.setArguments(QList<QVariant>() << "org.kubuntu.qaptworker.transaction");

    QDBusReply<QVariantMap> reply = QDBusConnection::systemBus().call(call);

    QVariantMap propertyMap = reply.value();

    if (propertyMap.isEmpty())
        return;

    for (auto iter = propertyMap.constBegin(); iter != propertyMap.constEnd(); ++iter) {
        if (!setProperty(iter.key().toLatin1(), iter.value()))
            qDebug() << "failed to set:" << iter.key();
    }
}

void Transaction::updateProperty(int type, const QDBusVariant &variant)
{
    switch (type) {
    case TransactionIdProperty:
        break; // Read-only
    case UserIdProperty:
        setUserId(variant.variant().toInt());
        break;
    default:
        break;
    }
}

}
