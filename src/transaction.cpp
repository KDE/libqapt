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
            , role(EmptyRole)
            , status(QApt::SetupStatus)
            , error(QApt::Success)
            , exitStatus(QApt::ExitUnfinished)
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
        TransactionRole role;
        TransactionStatus status;
        ErrorCode error;
        ExitStatus exitStatus;
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

void Transaction::updateUserId(int uid)
{
    d->uid = uid;
}

QApt::TransactionRole Transaction::role() const
{
    return d->role;
}

void Transaction::updateRole(QApt::TransactionRole role)
{
    d->role = role;
}

QApt::TransactionStatus Transaction::status() const
{
    return d->status;
}

void Transaction::updateStatus(TransactionStatus status)
{
    d->status = status;
}

QApt::ErrorCode Transaction::error() const
{
    return d->error;
}

void Transaction::updateError(ErrorCode error)
{
    d->error = error;
}

QApt::ExitStatus Transaction::exitStatus() const
{
    return d->exitStatus;
}

void Transaction::updateExitStatus(ExitStatus exitStatus)
{
    d->exitStatus = exitStatus;
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
        if (!setProperty(iter.key().toLatin1(), iter.value())) {
            // Qt won't support arbitrary enums over dbus until "maybe Qt 6 or 7"
            // when c++11 is prevalent, so do some ugly hacks here...
            if (iter.key() == QLatin1String("role"))
                setProperty(iter.key().toLatin1(), (TransactionRole)iter.value().toInt());
            else if (iter.key() == QLatin1String("status"))
                setProperty(iter.key().toLatin1(), (TransactionStatus)iter.value().toInt());
            else if (iter.key() == QLatin1String("error"))
                setProperty(iter.key().toLatin1(), (ErrorCode)iter.value().toInt());
            else if (iter.key() == QLatin1String("exitStatus"))
                setProperty(iter.key().toLatin1(), (ExitStatus)iter.value().toInt());
            else
                qDebug() << "failed to set:" << iter.key();
        }
    }
}

void Transaction::updateProperty(int type, const QDBusVariant &variant)
{
    switch (type) {
    case TransactionIdProperty:
        break; // Read-only
    case UserIdProperty:
        updateUserId(variant.variant().toInt());
        break;
    case RoleProperty:
        updateRole((TransactionRole)variant.variant().toInt());
        break;
    case StatusProperty:
        updateStatus((TransactionStatus)variant.variant().toInt());
        break;
    case ErrorProperty:
        updateError((ErrorCode)variant.variant().toInt());
        break;
    case ExitStatusProperty:
        updateExitStatus((ExitStatus)variant.variant().toInt());
    default:
        break;
    }
}

}
