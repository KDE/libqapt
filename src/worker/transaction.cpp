/***************************************************************************
 *   Copyright © 2012 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *   Copyright © 2008-2009 Sebastian Heinlein <devel@glatzor.de>           *
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
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

// D-Bus/Polkit includes
#include "qaptauthorization.h"
#include "transactionadaptor.h"

Transaction::Transaction(QObject *parent, QQueue<Transaction *> *queue, int userId)
    : Transaction(parent, QApt::EmptyRole, queue, userId, QVariantMap())
{
}

Transaction::Transaction(QObject *parent, QApt::TransactionRole role,
                         QQueue<Transaction *> *queue, int userId, QVariantMap packagesList)
    : QObject(parent)
    , m_queue(queue)
    , m_tid(QUuid::createUuid().toString())
    , m_uid(userId)
    , m_role(role)
    , m_status(QApt::WaitingStatus)
    , m_packages(packagesList)
    , m_cancellable(true)
    , m_cancelled(false)
    , m_exitStatus(QApt::ExitUnfinished)
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

void Transaction::setStatus(QApt::TransactionStatus status)
{
    m_status = status;
    emit propertyChanged(QApt::StatusProperty, QDBusVariant((int)status));
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

QVariantMap Transaction::packages() const
{
    return m_packages;
}

void Transaction::setPackages(QVariantMap packageList)
{
    if (m_status != QApt::SetupStatus) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));
        return;
    }

    m_packages = packageList;
    emit propertyChanged(QApt::PackagesProperty, QDBusVariant(packageList));
}

bool Transaction::cancellable() const
{
    return m_cancellable;
}

void Transaction::setCancellable(bool cancellable)
{
    m_cancellable = cancellable;
    emit propertyChanged(QApt::CancellableProperty, QDBusVariant(cancellable));
}

bool Transaction::cancelled() const
{
    return m_cancelled;
}

int Transaction::exitStatus() const
{
    return (int)m_exitStatus;
}

void Transaction::setExitStatus(QApt::ExitStatus exitStatus)
{
    setStatus(QApt::FinishedStatus);
    m_exitStatus = exitStatus;
    emit propertyChanged(QApt::ExitStatusProperty, QDBusVariant(exitStatus));
    emit finished(exitStatus);

    // TODO: Set deletion timeout (5s)
}

QString Transaction::medium() const
{
    return m_medium;
}

void Transaction::setMediumRequired(const QString &label, const QString &medium)
{
    m_medium = medium;
    m_paused = true;

    emit mediumRequired(label, medium);
}

bool Transaction::paused() const
{
    return m_paused;
}

QString Transaction::statusDetails() const
{
    return m_statusDetails;
}

void Transaction::setStatusDetails(const QString &details)
{
    m_statusDetails = details;

    emit propertyChanged(QApt::StatusDetailsProperty, QDBusVariant(details));
}

int Transaction::progress() const
{
    return m_progress;
}

void Transaction::setProgress(int progress)
{
    m_progress = progress;

    emit propertyChanged(QApt::ProgressProperty, QDBusVariant(progress));
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
        setRole((QApt::TransactionProperty)value.variant().toInt());
        break;
    case QApt::StatusProperty:
        setStatus((QApt::TransactionStatus)value.variant().toInt());
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
    case QApt::PackagesProperty:
        setPackages(value.variant().toMap());
        break;
    default:
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::InvalidArgs, QString()));
        break;
    }
}

void Transaction::cancel()
{
    if (isForeignUser()) {
        if (!QApt::Auth::authorize(QLatin1String("org.kubuntu.qaptworker.foreignCancel"),
                                   QLatin1String("org.kubuntu.qaptworker"))) {
            QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::AccessDenied, QString()));
            return;
        }
    }

    // We can only cancel cancellable transactions, obviously
    if (!m_cancellable) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));
        return;
    }

    m_cancelled = true;
    m_paused = false;
    emit propertyChanged(QApt::CancelledProperty, QDBusVariant(m_cancelled));
}

void Transaction::provideMedium(const QString &medium)
{
    if (isForeignUser()) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::AccessDenied, QString()));
        return;
    }

    // An incorrect medium was provided, or no medium was requested
    if (medium != m_medium || m_medium.isEmpty()) {
        QDBusConnection::systemBus().send(QDBusMessage::createError(QDBusError::Failed, QString()));
        return;
    }

    // The medium has now been provided, and the installation should be able to continue
    m_paused = false;
}
