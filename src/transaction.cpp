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
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>

#include <QDebug>

// Own includes
#include "dbusinterfaces_p.h"

namespace QApt {

class TransactionPrivate
{
    public:
        TransactionPrivate(const QString &id)
            : tid(id)
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
            dbus = new TransactionInterface(QLatin1String(s_workerReverseDomainName),
                                            tid, QDBusConnection::systemBus(),
                                            0);
        }

        ~TransactionPrivate()
        {
            delete dbus;
        }

        // DBus
        TransactionInterface *dbus;
        QDBusServiceWatcher *watcher;

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
        QString filePath;
        QString errorDetails;
        QApt::FrontendCaps frontendCaps;
};

Transaction::Transaction(const QString &tid)
    : QObject()
    , d(new TransactionPrivate(tid))
{
    // Fetch property data from D-Bus
    sync();

    d->watcher = new QDBusServiceWatcher(this);
    d->watcher->setConnection(QDBusConnection::systemBus());
    d->watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    d->watcher->addWatchedService(QLatin1String(s_workerReverseDomainName));

    connect(d->dbus, SIGNAL(propertyChanged(int,QDBusVariant)),
            this, SLOT(updateProperty(int,QDBusVariant)));
    connect(d->dbus, SIGNAL(mediumRequired(QString,QString)),
            this, SIGNAL(mediumRequired(QString,QString)));
    connect(d->dbus, SIGNAL(promptUntrusted(QStringList)),
            this, SIGNAL(promptUntrusted(QStringList)));
    connect(d->dbus, SIGNAL(configFileConflict(QString,QString)),
            this, SIGNAL(configFileConflict(QString,QString)));
    connect(d->watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(serviceOwnerChanged(QString,QString,QString)));
}

Transaction::~Transaction()
{
    delete d;
}

bool Transaction::operator==(const Transaction* rhs) const
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

QString Transaction::errorString() const
{
    QString text;

    if (this->error() == QApt::ErrorCode::Success) {
        return QString();
    }
    
    switch (this->error()) {
    case QApt::ErrorCode::Success:
    case QApt::ErrorCode::UnknownError:
        text = tr(
            "An unknown error has occurred, here are the details: %1",
            "error"
        ).arg( this->errorDetails() );
        break;
        
    case QApt::ErrorCode::InitError:
        text = tr(
            "The package system could not be initialized, your "
            "configuration may be broken.",
            "error"
        );
        break;
        
    case QApt::ErrorCode::LockError:
        text = tr(
            "Another application seems to be using the package "
            "system at this time. You must close all other package "
            "managers before you will be able to install or remove "
            "any packages.",
            "error"
        );
        break;

    case QApt::ErrorCode::DiskSpaceError:
        text = tr(
            "You do not have enough disk space in the directory "
            "at %1 to continue with this operation.",
            "error"
        ).arg( this->errorDetails() );
        break;
        
    case QApt::ErrorCode::FetchError:
        text = tr(
            "Could not download packages",
            "error"
        );
        break;
        
    case QApt::ErrorCode::CommitError:
        text = tr(
            "An error occurred while applying changes: %1",
            "error"
        ).arg( this->errorDetails() );
        break;
    
    case QApt::ErrorCode::AuthError:
        text = tr(
            "This operation cannot continue since proper "
            "authorization was not provided",
            "error"
        );
        break;
        
    case QApt::ErrorCode::WorkerDisappeared:
        text = tr(
            "It appears that the QApt worker has either crashed "
            "or disappeared. Please report a bug to the QApt maintainers",
            "error"
        );
        break;
        
    case QApt::ErrorCode::UntrustedError:
        text = tr(
            "The following package(s) has not been verified by its author(s). "
            "Downloading untrusted packages has been disallowed "
            "by your current configuration.",
            "error",
            this->untrustedPackages().size()
        );
        break;

    case QApt::ErrorCode::DownloadDisallowedError:
        text = tr("Downloads are currently disallowed.", "error");
        break;

    case QApt::ErrorCode::NotFoundError:
        text = tr("Error: Not Found", "error");
        break;

    case QApt::ErrorCode::WrongArchError:
        text = tr("Error: Wrong architecture", "error");
        break;

    case QApt::ErrorCode::MarkingError:
        text = tr("Error: Marking error", "error");
        break;
    }

    return text;

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
    if (d->progress != progress) {
        d->progress = progress;
        emit progressChanged(d->progress);
    }
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

QString Transaction::filePath() const
{
    return d->filePath;
}

void Transaction::updateFilePath(const QString &filePath)
{
    d->filePath = filePath;
}

QString Transaction::errorDetails() const
{
    return d->errorDetails;
}

QApt::FrontendCaps Transaction::frontendCaps() const
{
    return d->frontendCaps;
}

void Transaction::updateErrorDetails(const QString &errorDetails)
{
    d->errorDetails = errorDetails;
}

void Transaction::setLocale(const QString &locale)
{
    QDBusPendingCall call = d->dbus->setProperty(QApt::LocaleProperty,
                                                 QDBusVariant(locale));

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::setFrontendCaps(FrontendCaps frontendCaps)
{
    QDBusPendingCall call = d->dbus->setProperty(QApt::FrontendCapsProperty,
                                                 QDBusVariant((int)frontendCaps));

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::updateFrontendCaps(FrontendCaps frontendCaps)
{
    d->frontendCaps = frontendCaps;
}

void Transaction::setProxy(const QString &proxy)
{
    QDBusPendingCall call = d->dbus->setProperty(QApt::ProxyProperty,
                                                 QDBusVariant(proxy));

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::setDebconfPipe(const QString &pipe)
{
    QDBusPendingCall call = d->dbus->setProperty(QApt::DebconfPipeProperty,
                                                 QDBusVariant(pipe));

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

void Transaction::resolveConfigFileConflict(const QString &currentPath, bool replace)
{
    QDBusPendingCall call = d->dbus->resolveConfigFileConflict(currentPath, replace);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
            this, SLOT(onCallFinished(QDBusPendingCallWatcher*)));
}

void Transaction::onCallFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<> reply = *watcher;

    if (reply.isError()) {
        qWarning() << "found error while replying" << reply.error();
        switch (reply.error().type())
        {
        case QDBusError::AccessDenied:
            updateError(QApt::AuthError);
            emit errorOccurred(QApt::AuthError);
            qWarning() << "auth error reply!";
            break;
        case QDBusError::NoReply:
            updateError(QApt::AuthError);
            emit errorOccurred(QApt::AuthError);
            qWarning() << "No reply error!";
            break;
        default:
            break;
        }
    }

    watcher->deleteLater();
}

void Transaction::serviceOwnerChanged(QString name, QString oldOwner, QString newOwner)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)

    if (newOwner.isEmpty() && d->exitStatus == QApt::ExitUnfinished) {
        updateError(QApt::WorkerDisappeared);
        emit errorOccurred(QApt::WorkerDisappeared);

        updateCancellable(false);
        emit cancellableChanged(false);

        updateStatus(QApt::FinishedStatus);
        emit statusChanged(QApt::FinishedStatus);

        updateExitStatus(QApt::ExitFailed);
        emit finished(exitStatus());
    }
}

void Transaction::sync()
{
    QString arg = QString("%1.%2").arg(QLatin1String(s_workerReverseDomainName),
                                       QLatin1String("transaction"));
    QDBusMessage call = QDBusMessage::createMethodCall(d->dbus->service(), d->tid,
                                                       "org.freedesktop.DBus.Properties", "GetAll");
    call.setArguments(QList<QVariant>() << arg);

    QDBusReply<QVariantMap> reply = QDBusConnection::systemBus().call(call);
    QVariantMap propertyMap = reply.value();

    if (propertyMap.isEmpty())
        return;

    for (auto iter = propertyMap.constBegin(); iter != propertyMap.constEnd(); ++iter) {
        if (!setProperty(iter.key().toLatin1(), iter.value())) {
            // Qt won't support arbitrary enums over dbus until "maybe Qt 6 or 7"
            // when c++11 is prevalent, so do some ugly hacks here...
            if (iter.key() == QLatin1String("role"))
                updateRole((TransactionRole)iter.value().toInt());
            else if (iter.key() == QLatin1String("status"))
                updateStatus((TransactionStatus)iter.value().toInt());
            else if (iter.key() == QLatin1String("error"))
                updateError((ErrorCode)iter.value().toInt());
            else if (iter.key() == QLatin1String("exitStatus"))
                updateExitStatus((ExitStatus)iter.value().toInt());
            else if (iter.key() == QLatin1String("packages"))
                // iter.value() for the QVariantMap is QDBusArgument, so we have to
                // set this manually
                setProperty(iter.key().toLatin1(), d->dbus->property(iter.key().toLatin1()));
            else if (iter.key() == QLatin1String("downloadProgress"))
                updateDownloadProgress(iter.value().value<QApt::DownloadProgress>());
            else if (iter.key() == QLatin1String("frontendCaps"))
                updateFrontendCaps((FrontendCaps)iter.value().toInt());
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
    case FilePathProperty:
        updateFilePath(variant.variant().toString());
        break;
    case ErrorDetailsProperty:
        updateErrorDetails(variant.variant().toString());
        break;
    case FrontendCapsProperty:
        updateFrontendCaps((FrontendCaps)variant.variant().toInt());
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
