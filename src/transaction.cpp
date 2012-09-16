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
#include <QtDBus/QDBusPendingCallWatcher>
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
            , isCancellable(true)
            , isCancelled(false)
            , exitStatus(QApt::ExitUnfinished)
            , isPaused(false)
            , progress(0)
            , downloadSpeed(0)
            , downloadETA(0)
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
        QString locale;
        QString proxy;
        QString debconfPipe;
        QVariantMap packages;
        bool isCancellable;
        bool isCancelled;
        ExitStatus exitStatus;
        bool isPaused;
        QString statusDetails;
        int progress;
        DownloadProgress downloadProgress;
        QStringList untrustedPackages;
        quint64 downloadSpeed;
        quint64 downloadETA;
};

Transaction::Transaction(const QString &tid)
    : QObject()
    , d(new TransactionPrivate(tid))
{
    // Fetch property data from D-Bus
    sync();

    connect(d->dbus, SIGNAL(propertyChanged(int,QDBusVariant)),
            this, SLOT(updateProperty(int,QDBusVariant)));
    connect(d->dbus, SIGNAL(mediumRequired(QString,QString)),
            this, SIGNAL(mediumRequired(QString,QString)));
    connect(d->dbus, SIGNAL(promptUntrusted(QStringList)),
            this, SIGNAL(promptUntrusted(QStringList)));
}

Transaction::Transaction(const Transaction *other)
{
    d = other->d;

    connect(d->dbus, SIGNAL(propertyChanged(int,QDBusVariant)),
            this, SLOT(updateProperty(int,QDBusVariant)));
    connect(d->dbus, SIGNAL(mediumRequired(QString,QString)),
            this, SIGNAL(mediumRequired(QString,QString)));
    connect(d->dbus, SIGNAL(promptUntrusted(QStringList)),
            this, SIGNAL(promptUntrusted(QStringList)));
}

Transaction::~Transaction()
{
}

bool Transaction::operator==(const Transaction* rhs)
{
    return d->tid == rhs->d->tid;
}

QString Transaction::transactionId() const
{
    return d->tid;
}

void Transaction::updateTransactionId(const QString &tid)
{
    d->tid = tid;
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

QString Transaction::locale() const
{
    return d->locale;
}

void Transaction::updateLocale(const QString &locale)
{
    d->locale = locale;
}

QString Transaction::proxy() const
{
    return d->proxy;
}

void Transaction::updateProxy(const QString &proxy)
{
    d->proxy = proxy;
}

QString Transaction::debconfPipe() const
{
    return d->debconfPipe;
}

void Transaction::updateDebconfPipe(const QString &pipe)
{
    d->debconfPipe = pipe;
}

QVariantMap Transaction::packages() const
{
    return d->packages;
}

void Transaction::updatePackages(const QVariantMap &packages)
{
    d->packages = packages;
}

bool Transaction::isCancellable() const
{
    return d->isCancellable;
}

void Transaction::updateCancellable(bool cancellable)
{
    d->isCancellable = cancellable;
}

bool Transaction::isCancelled() const
{
    return d->isCancelled;
}

void Transaction::updateCancelled(bool cancelled)
{
    d->isCancelled = cancelled;
}

QApt::ExitStatus Transaction::exitStatus() const
{
    return d->exitStatus;
}

void Transaction::updateExitStatus(ExitStatus exitStatus)
{
    d->exitStatus = exitStatus;
}

bool Transaction::isPaused() const
{
    return d->isPaused;
}

void Transaction::updatePaused(bool paused)
{
    d->isPaused = paused;
}

QString Transaction::statusDetails() const
{
    return d->statusDetails;
}

void Transaction::updateStatusDetails(const QString &details)
{
    d->statusDetails = details;
}

int Transaction::progress() const
{
    return d->progress;
}

void Transaction::updateProgress(int progress)
{
    d->progress = progress;
}

DownloadProgress Transaction::downloadProgress() const
{
    return d->downloadProgress;
}

void Transaction::updateDownloadProgress(const DownloadProgress &downloadProgress)
{

    d->downloadProgress = downloadProgress;
}

QStringList Transaction::untrustedPackages() const
{
    return d->untrustedPackages;
}

void Transaction::updateUntrustedPackages(const QStringList &untrusted)
{
    d->untrustedPackages = untrusted;
}

quint64 Transaction::downloadSpeed() const
{
    return d->downloadSpeed;
}

void Transaction::updateDownloadSpeed(quint64 downloadSpeed)
{
    d->downloadSpeed = downloadSpeed;
}

quint64 Transaction::downloadETA() const
{
    return d->downloadETA;
}

void Transaction::updateDownloadETA(quint64 ETA)
{
    d->downloadETA = ETA;
}

void Transaction::setLocale(const QString &locale)
{
    QDBusPendingCall call = d->dbus->setProperty(QApt::LocaleProperty,
                                                 QDBusVariant(locale));

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::setProxy(const QString &proxy)
{
    QDBusPendingCall call = d->dbus->setProperty(QApt::ProxyProperty,
                                                 QDBusVariant(proxy));

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::run()
{
    QDBusPendingCall call = d->dbus->run();

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::cancel()
{
    QDBusPendingCall call = d->dbus->cancel();

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::provideMedium(const QString &medium)
{
    QDBusPendingCall call = d->dbus->provideMedium(medium);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::replyUntrustedPrompt(bool approved)
{
    QDBusPendingCall call = d->dbus->replyUntrustedPrompt(approved);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::onCallFinished(QDBusPendingCallWatcher *watcher)
{
    qDebug() << "reply";
    QDBusPendingReply<> reply = *watcher;

    if (reply.isError()) {
        qDebug() << reply.error();
        switch (reply.error().type())
        {
        case QDBusError::AccessDenied:
            emit errorOccurred(QApt::AuthError);
            break;
        default:
            emit errorOccurred(QApt::UnknownError);
            break;
        }
    }

    watcher->deleteLater();
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
            else if (iter.key() == QLatin1String("packages"))
                // iter.value() for the QVariantMap is QDBusArgument, so we have to
                // set this manually
                setProperty(iter.key().toLatin1(), d->dbus->property(iter.key().toLatin1()));
            else if (iter.key() == "downloadProgress") {
                QVariant v = d->dbus->property(iter.key().toLatin1());
                updateDownloadProgress(v.value<QApt::DownloadProgress>());
            }
            else
                qDebug() << "failed to set:" << iter.key();
        }
    }
}

void Transaction::updateProperty(int type, const QDBusVariant &variant)
{
    switch (type) {
    case TransactionIdProperty:
        break; // Read-only after it has been set
    case UserIdProperty:
        break; // Read-only after it has been set
    case RoleProperty:
        updateRole((TransactionRole)variant.variant().toInt());
        break;
    case StatusProperty:
        updateStatus((TransactionStatus)variant.variant().toInt());
        emit statusChanged(status());
        break;
    case ErrorProperty:
        updateError((ErrorCode)variant.variant().toInt());
        emit errorOccurred(error());
        break;
    case LocaleProperty:
        updateLocale(variant.variant().toString());
        break;
    case ProxyProperty:
        updateProxy(variant.variant().toString());
        break;
    case DebconfPipeProperty:
        updateDebconfPipe(variant.variant().toString());
        break;
    case PackagesProperty:
        updatePackages(variant.variant().toMap());
        break;
    case CancellableProperty:
        updateCancellable(variant.variant().toBool());
        emit cancellableChanged(isCancellable());
        break;
    case CancelledProperty:
        updateCancelled(variant.variant().toBool());
        break;
    case ExitStatusProperty:
        updateExitStatus((ExitStatus)variant.variant().toInt());
        if (exitStatus() != QApt::ExitUnfinished)
            emit finished(exitStatus());
        break;
    case PausedProperty:
        updatePaused(variant.variant().toBool());
        if (isPaused())
            emit paused();
        else
            emit resumed();
        break;
    case StatusDetailsProperty:
        updateStatusDetails(variant.variant().toString());
        emit statusDetailsChanged(statusDetails());
        break;
    case ProgressProperty:
        updateProgress(variant.variant().toInt());
        emit progressChanged(progress());
        break;
    case DownloadProgressProperty: {
        QApt::DownloadProgress prog;
        QDBusArgument arg = variant.variant().value<QDBusArgument>();
        arg >> prog;

        updateDownloadProgress(prog);
        emit downloadProgressChanged(downloadProgress());
        break;
    }
    case UntrustedPackagesProperty:
        updateUntrustedPackages(variant.variant().toStringList());
        break;
    case DownloadSpeedProperty:
        updateDownloadSpeed(variant.variant().toULongLong());
        emit downloadSpeedChanged(downloadSpeed());
        break;
    case DownloadETAProperty:
        updateDownloadETA(variant.variant().toULongLong());
        emit downloadETAChanged(downloadETA());
        break;
    default:
        break;
    }
}

void Transaction::emitFinished(int exitStatus)
{
    emit finished((QApt::ExitStatus)exitStatus);
}

}
