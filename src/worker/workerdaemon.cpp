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
#include <QtCore/QThread>

// Own includes
#include "aptworker.h"
#include "qaptworkeradaptor.h"
#include "transaction.h"
#include "transactionqueue.h"

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

    // Invoke with Qt::QueuedConnection since the Qt event loop isn't up yet
    QMetaObject::invokeMethod(m_worker, "init", Qt::QueuedConnection);
    connect(m_queue, SIGNAL(queueChanged(QString,QStringList)),
            this, SIGNAL(transactionQueueChanged(QString,QStringList)),
            Qt::QueuedConnection);

    // Start up D-Bus service
    new QaptworkerAdaptor(this);

    if (!QDBusConnection::systemBus().registerService(QLatin1String("org.kubuntu.qaptworker"))) {
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
}

WorkerDaemon::~WorkerDaemon()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

int WorkerDaemon::dbusSenderUid() const
{
    return connection().interface()->serviceUid(message().service()).value();
}

Transaction *WorkerDaemon::createTranscation(QApt::TransactionRole role, QVariantMap instructionsList)
{
    int uid = dbusSenderUid();
    // Create a transaction. It will add itself to the queue

    return new Transaction(m_queue, uid, role, instructionsList);;
}

QString WorkerDaemon::updateCache()
{
    Transaction *trans = createTranscation(QApt::UpdateCacheRole);

    return trans->transactionId();
}

QString WorkerDaemon::installFile(const QString &file)
{
    Transaction *trans = createTranscation(QApt::InstallFileRole);
    // FIXME: Add a "filePath" property to Transaction, and set it here

    return trans->transactionId();
}

QString WorkerDaemon::commitChanges(QVariantMap instructionsList)
{
    Transaction *trans = createTranscation(QApt::CommitChangesRole,
                                           instructionsList);

    return trans->transactionId();
}

QString WorkerDaemon::updateXapianIndex()
{
    Transaction *trans = createTranscation(QApt::UpdateXapianRole);

    return trans->transactionId();
}

bool WorkerDaemon::writeFileToDisk(const QString &contents, const QString &path)
{
    // TODO: Put impl. in AptWorker?

    return false;
}

bool WorkerDaemon::copyArchiveToCache(const QString &archivePath)
{
    // TODO: Put impl. in AptWorker?

    return false;
}
