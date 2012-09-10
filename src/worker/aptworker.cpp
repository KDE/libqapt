/***************************************************************************
 *   Copyright © 2010-2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
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

#include "aptworker.h"

// Qt includes
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtCore/QDebug>

// Apt-pkg includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <string>

// Own includes
#include "aptlock.h"
#include "cache.h"
#include "config.h"
#include "transaction.h"
#include "workeracquire.h"

class CacheOpenProgress : public OpProgress
{
public:
    CacheOpenProgress(Transaction *trans = nullptr,
                      int begin = 0, int end = 100)
        : m_trans(trans)
        , m_begin(begin)
        , m_lastProgress(0)
    {
        // APT opens the cache in 4 "steps", and the progress bar will go to 100
        // 4 times. As such, we use modifiers for the current step to show progress
        // relative to the whole job of opening the cache
        QList<qreal> modifiers;
        modifiers << 0.25 << 0.50 << 0.75 << 1.0;

        for (qreal modifier : modifiers) {
            m_steps.append((qreal)begin + ((qreal)end - (qreal)begin) * modifier);
        }

        m_end = m_steps.takeFirst();
    }

    void Update() {
        int progress;
        if (Percent < 101) {
            progress = qRound(m_begin + (Percent / 100) * (m_end - m_begin));
            if (m_lastProgress == progress)
                        return;
        } else {
            progress = 101;
        }

        m_lastProgress = progress;
        if (m_trans)
            m_trans->setProgress(progress);
    }

    void Done() {
        // Make sure the progress is set correctly
        m_lastProgress = m_end;
        // Update beginning progress for the next "step"
        m_begin = m_end;

        if (m_steps.size())
            m_end = m_steps.takeFirst();
    }

private:
    Transaction *m_trans;
    QList<qreal> m_steps;
    qreal m_begin;
    qreal m_end;
    int m_lastProgress;
};

AptWorker::AptWorker(QObject *parent)
    : QObject(parent)
    , m_cache(nullptr)
    , m_records(nullptr)
    , m_trans(nullptr)
    , m_ready(false)
    , m_lastActiveTimestamp(QDateTime::currentMSecsSinceEpoch())
{
}

AptWorker::~AptWorker()
{
    delete m_cache;
    delete m_records;
    qDeleteAll(m_locks);
}

Transaction *AptWorker::currentTransaction() const
{
    return m_trans;
}

quint64 AptWorker::lastActiveTimestamp() const
{
    return m_lastActiveTimestamp;
}

void AptWorker::init()
{
    if (m_ready)
        return;

    pkgInitConfig(*_config);
    pkgInitSystem(*_config, _system);
    m_cache = new pkgCacheFile;

    // Prepare locks to be used later
    QApt::Config *config = new QApt::Config(this);
    QStringList dirs;
    dirs << config->findDirectory("Dir::Cache::Archives")
         << config->findDirectory("Dir::State::lists");

    QString statusFile = config->findDirectory("Dir::State::status");
    QFileInfo info(statusFile);
    dirs << info.dir().absolutePath();

    for (const QString &dir : dirs) {
        m_locks << new AptLock(dir);
    }

    m_ready = true;
}

void AptWorker::runTransaction(Transaction *trans)
{
    // Check for running transactions or uninitialized worker
    if (m_trans || !m_ready)
        return;

    m_lastActiveTimestamp = QDateTime::currentMSecsSinceEpoch();
    m_trans = trans;
    trans->setStatus(QApt::RunningStatus);
    waitForLocks();

    // Process transactions requiring no cache
    switch (trans->role()) {
    case QApt::UpdateXapianRole:
        break;
    default: // The transaction must need a cache
        openCache();
        break;
    }

    // Process transactions requiring a cache
    switch (trans->role()) {
    // Transactions that can use a broken cache
    case QApt::UpdateCacheRole:
        updateCache();
        break;
    // Transactions that need a consistent cache
    case QApt::UpgradeSystemRole:
        //upgradeSystem();
        break;
    // Other
    case QApt::EmptyRole:
    default:
        break;
    }

    // Cleanup
    cleanupCurrentTransaction();
}

void AptWorker::cleanupCurrentTransaction()
{
    // Well, we're finished now.
    m_trans->setProgress(100);

    // Release locks
    for (AptLock *lock : m_locks) {
        lock->release();
    }

    // Set transaction exit status
    // This will notify the transaction queue of the transaction's completion
    // as well as mark the transaction for deletion in 5 seconds
    if (m_trans->isCancelled())
        m_trans->setExitStatus(QApt::ExitCancelled);
    else if (m_trans->error() != QApt::Success)
        m_trans->setExitStatus(QApt::ExitFailed);
    else
        m_trans->setExitStatus(QApt::ExitSuccess);

    m_trans = nullptr;
    m_lastActiveTimestamp = QDateTime::currentMSecsSinceEpoch();
}

void AptWorker::waitForLocks()
{
    for (AptLock *lock : m_locks) {
        if (lock->acquire()) {
            qDebug() << "locked?" << lock->isLocked();
            continue;
        }

        // Couldn't get lock
        m_trans->setIsPaused(true);
        m_trans->setStatus(QApt::WaitingLockStatus);

        while (!lock->isLocked() && m_trans->isPaused() && !m_trans->isCancelled()) {
            // Wait 3 seconds and try again
            sleep(3);
            lock->acquire();
        }

        m_trans->setIsPaused(false);
    }
}

void AptWorker::openCache(int begin, int end)
{
    m_trans->setStatus(QApt::LoadingCacheStatus);
    CacheOpenProgress *progress = new CacheOpenProgress(m_trans, begin, end);
    if (!m_cache->ReadOnlyOpen(progress)) {
        std::string message;
        bool isError = _error->PopMessage(message);
        if (isError)
            qWarning() << QString::fromStdString(message);

        delete progress;
        return;
    }

    delete progress;
    delete m_records;
    m_records = new pkgRecords(*(m_cache));
}

void AptWorker::updateCache()
{
    WorkerAcquire *acquire = new WorkerAcquire(this, 10, 90);
    acquire->setTransaction(m_trans);

    // Initialize fetcher with our progress watcher
    pkgAcquire fetcher;
    fetcher.Setup(acquire);

    // Fetch the lists.
    if (!ListUpdate(*acquire, *m_cache->GetSourceList())) {
        if (!m_trans->isCancelled()) {
            m_trans->setError(QApt::FetchError);
        }
    }

    // Clean up
    delete acquire;

    openCache(91, 95);
}
