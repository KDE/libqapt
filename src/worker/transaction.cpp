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

#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

#include "transactionadaptor.h"

Transaction::Transaction(QObject *parent, QQueue<Transaction *> *queue, int userId)
    : Transaction(parent, QApt::EmptyRole, queue, userId)
{
}

Transaction::Transaction(QObject *parent, QApt::TransactionRole role,
                         QQueue<Transaction *> *queue, int userId)
    : QObject(parent)
    , m_queue(queue)
    , m_tid(QUuid::createUuid().toString())
    , m_uid(userId)
    , m_role(role)
    , m_status(QApt::WaitingStatus)
{
    new TransactionAdaptor(this);
    QDBusConnection connection = QDBusConnection::systemBus();

    QString tid = QUuid::createUuid().toString();
    // Remove parts of the uuid that can't be part of a D-Bus path
    tid.remove('{').remove('}').remove('-');
    m_tid = "/org/kubuntu/qaptworker/transaction" + tid;

    if (!connection.registerObject(m_tid, this))
        qWarning() << "Unable to register transaction on DBus";
}

QString Transaction::transactionId() const
{
    return m_tid;
}

int Transaction::userId() const
{
    return m_uid;
}

int Transaction::role() const
{
    return m_role;
}

void Transaction::setRole(int role)
{
    // Cannot change role for an already determined transaction
    if (m_role != QApt::EmptyRole) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));

        return;
    }

    m_role = (QApt::TransactionRole)role;

    emit propertyChanged(QApt::RoleProperty, QDBusVariant(role));
}

int Transaction::status() const
{
    return m_status;
}

void Transaction::setStatus(int status)
{
    m_status = (QApt::TransactionStatus)status;
    emit propertyChanged(QApt::StatusProperty, QDBusVariant(status));
}

QString Transaction::locale() const
{
    return m_locale;
}

void Transaction::setLocale(QString locale)
{
    if (m_status != QApt::SetupStatus) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));

        return;
    }

    m_locale = locale;
    emit propertyChanged(QApt::LocaleProperty, QDBusVariant(locale));
}

QString Transaction::proxy() const
{
    return m_locale;
}

void Transaction::setProxy(QString proxy)
{
    if (m_status != QApt::SetupStatus) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));

        return;
    }

    m_proxy = proxy;
    emit propertyChanged(QApt::ProxyProperty, QDBusVariant(proxy));
}

QString Transaction::debconfPipe() const
{
    return m_debconfPipe;
}

void Transaction::setDebconfPipe(QString pipe)
{
    if (m_status != QApt::SetupStatus) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));
        return;
    }

    QFileInfo pipeInfo(pipe);

    if (!pipeInfo.exists() || pipeInfo.ownerId() != m_uid) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::InvalidArgs, QString()));
        return;
    }

    m_debconfPipe = pipe;
    emit propertyChanged(QApt::DebconfPipeProperty, QDBusVariant(pipe));
}

void Transaction::run()
{
    if (isForeignUser()) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::AccessDenied, QString()));

        return;
    }

    // Place on queue
}

int Transaction::dbusSenderUid() const
{
    return connection().interface()->serviceUid( message().service()).value();
}

bool Transaction::isForeignUser() const
{
    return dbusSenderUid() != m_uid;
}

void Transaction::setProperty(int property, QDBusVariant value)
{
    if (isForeignUser()) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::AccessDenied, QString()));

        return;
    }

    switch (property)
    {
    case QApt::TransactionIdProperty:
    case QApt::UserIdProperty:
        // Read-only
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));
        break;
    case QApt::RoleProperty:
        setRole(value.variant().toInt());
        break;
    case QApt::StatusProperty:
        setStatus(value.variant().toInt());
        break;
    case QApt::LocaleProperty:
        setLocale(value.variant().toString());
        break;
    case QApt::ProxyProperty:
        setProxy(value.variant().toString());
        break;
    case QApt::DebconfPipeProperty:
        setDebconfPipe(value.variant().toString());
        break;
    default:
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::InvalidArgs, QString()));
        break;
    }
}
