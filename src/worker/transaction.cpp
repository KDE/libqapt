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
#include <QDebug>
#include <QDBusError>

#include "transactionadaptor.h"

Transaction::Transaction(QObject *parent, QQueue<Transaction *> *queue)
    : Transaction(parent, QApt::EmptyRole, queue)
{
}

Transaction::Transaction(QObject *parent, QApt::TransactionRole role,
                         QQueue<Transaction *> *queue)
    : QObject(parent)
    , m_queue(queue)
    , m_tid(QUuid::createUuid().toString())
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

int Transaction::role() const
{
    return m_role;
}

bool Transaction::setRole(int role)
{
    // Cannot change role for an already determined transaction
    if (m_role != QApt::EmptyRole)
        return false;

    m_role = (QApt::TransactionRole)role;

    emit propertyChanged(QApt::RoleProperty, QDBusVariant(role));
    return true;
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

void Transaction::run()
{
    // Place on queue
}
