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

#include "workerdaemon.h"

// Qt includes
#include <QThread>
#include <QTimer>

// Apt-pkg includes
#include <apt-pkg/configuration.h>

// Own includes
#include "aptworker.h"
#include "qaptauthorization.h"
#include "transaction.h"
#include "transactionqueue.h"
#include "workeradaptor.h"
#include "urihelper.h"

#define IDLE_TIMEOUT 30000 // 30 seconds

WorkerDaemon::WorkerDaemon(int &argc, char **argv)
    : QCoreApplication(argc, argv)
    , m_queue(nullptr)
    , m_worker(nullptr)
    , m_workerThread(nullptr)
{
    m_worker = new AptWorker(nullptr);
    m_queue = new TransactionQueue(this, m_worker);

    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);
    m_workerThread->start();
    connect(m_workerThread, SIGNAL(finished()), this, SLOT(quit()));

    // Invoke with Qt::QueuedConnection since the Qt event loop isn't up yet
    QMetaObject::invokeMethod(m_worker, "init", Qt::QueuedConnection);
    connect(m_queue, SIGNAL(queueChanged(QString,QStringList)),
            this, SIGNAL(transactionQueueChanged(QString,QStringList)),
            Qt::QueuedConnection);
    qRegisterMetaType<Transaction *>("Transaction *");
    QApt::DownloadProgress::registerMetaTypes();

    // Start up D-Bus service
    new WorkerAdaptor(this);

    if (!QDBusConnection::systemBus().registerService(QLatin1String(s_workerReverseDomainName))) {
        // Another worker is already here, quit
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        qWarning() << "Couldn't register service" << QDBusConnection::systemBus().lastError().message();
        return;
    }

    if (!QDBusConnection::systemBus().registerObject(QLatin1String("/"), this)) {
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        qWarning() << "Couldn't register object";
        return;
    }

    // Quit if we've not run a job for a while
    m_idleTimer = new QTimer(this);
    m_idleTimer->start(IDLE_TIMEOUT);
    connect(m_idleTimer, SIGNAL(timeout()), this, SLOT(checkIdle()), Qt::QueuedConnection);
}

void WorkerDaemon::checkIdle()
{
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (!m_worker->currentTransaction() &&
        currentTime - m_worker->lastActiveTimestamp() > IDLE_TIMEOUT &&
        m_queue->isEmpty()) {
        m_worker->quit();
    }
}

int WorkerDaemon::dbusSenderUid() const
{
    return connection().interface()->serviceUid(message().service()).value();
}

Transaction *WorkerDaemon::createTransaction(QApt::TransactionRole role, QVariantMap instructionsList)
{
    int uid = dbusSenderUid();

    // Create a transaction. It will add itself to the queue
    Transaction *trans = new Transaction(m_queue, uid, role, instructionsList);
    trans->setService(message().service());

    return trans;
}

QString WorkerDaemon::updateCache()
{
    Transaction *trans = createTransaction(QApt::UpdateCacheRole);

    return trans->transactionId();
}

QString WorkerDaemon::installFile(const QString &file)
{
    Transaction *trans = createTransaction(QApt::InstallFileRole);
    trans->setFilePath(file);

    return trans->transactionId();
}

QString WorkerDaemon::commitChanges(QVariantMap instructionsList)
{
    Transaction *trans = createTransaction(QApt::CommitChangesRole,
                                           instructionsList);

    return trans->transactionId();
}

QString WorkerDaemon::upgradeSystem(bool safeUpgrade)
{
    Transaction *trans = createTransaction(QApt::UpgradeSystemRole);
    trans->setSafeUpgrade(safeUpgrade);

    return trans->transactionId();
}

QString WorkerDaemon::downloadArchives(const QStringList &packageNames, const QString &dest)
{
    QVariantMap packages;

    for (const QString &pkg : packageNames) {
        packages.insert(pkg, 0);
    }

    Transaction *trans = createTransaction(QApt::DownloadArchivesRole, packages);
    trans->setFilePath(dest);

    return trans->transactionId();
}

bool WorkerDaemon::writeFileToDisk(const QString &contents, const QString &path)
{
    if (!QApt::Auth::authorize(dbusActionUri("writefiletodisk"), message().service())) {
        qDebug() << "Failed to authorize!!";
        return false;
    }

    QFile file(path);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(contents.toLatin1());
        return true;
    }

    qDebug() << "Failed to write file to disk: " << file.errorString();
    return false;
}

bool WorkerDaemon::copyArchiveToCache(const QString &archivePath)
{
    if (!QApt::Auth::authorize(dbusActionUri("writefiletodisk"), message().service())) {
        return false;
    }

    QString cachePath = QString::fromStdString(_config->FindDir("Dir::Cache::Archives"));
    // Filename
    cachePath += archivePath.right(archivePath.size() - archivePath.lastIndexOf('/'));

    if (QFile::exists(cachePath)) {
        // Already copied
        return true;
    }

    return QFile::copy(archivePath, cachePath);
}
