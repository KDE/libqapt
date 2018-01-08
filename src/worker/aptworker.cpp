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
#include <QtCore/QStringBuilder>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QDebug>

// Apt-pkg includes
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/algorithms.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <string>

// System includes
#include <sys/statvfs.h>
#include <sys/statfs.h>
#define RAMFS_MAGIC     0x858458f6

// Own includes
#include "aptlock.h"
#include "cache.h"
#include "debfile.h"
#include "package.h"
#include "transaction.h"
#include "workeracquire.h"
#include "workerinstallprogress.h"

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
            progress = qRound(m_begin + qreal(Percent / 100.0) * (m_end - m_begin));
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

void AptWorker::quit()
{
    thread()->quit();
}

Transaction *AptWorker::currentTransaction()
{
    QMutexLocker locker(&m_transMutex);
    return m_trans;
}

quint64 AptWorker::lastActiveTimestamp()
{
    QMutexLocker locker(&m_timestampMutex);
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
    QStringList dirs;

    dirs << QString::fromStdString(_config->FindDir("Dir::Cache::Archives"))
         << QString::fromStdString(_config->FindDir("Dir::State::lists"));

    QString statusFile = QString::fromStdString(_config->FindDir("Dir::State::status"));
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

    m_timestampMutex.lock();
    m_lastActiveTimestamp = QDateTime::currentMSecsSinceEpoch();
    m_timestampMutex.unlock();
    m_trans = trans;
    trans->setStatus(QApt::RunningStatus);
    waitForLocks();
    openCache();

    // Check for init error
    if (m_trans->error() != QApt::Success) {
        cleanupCurrentTransaction();
        return;
    }

    // Process transactions requiring a cache
    switch (trans->role()) {
    // Transactions that can use a broken cache
    case QApt::UpdateCacheRole:
        updateCache();
        break;
    // Transactions that need a consistent cache
    case QApt::UpgradeSystemRole:
        upgradeSystem();
        break;
    case QApt::CommitChangesRole:
        if (markChanges())
            commitChanges();
        break;
    case QApt::InstallFileRole:
        installFile();
        m_dpkgProcess->waitForFinished(-1);
        break;
    case QApt::DownloadArchivesRole:
        downloadArchives();
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

    QMutexLocker locker(&m_timestampMutex);
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

    // Close in case it's already open
    m_cache->Close();
    _error->Discard();
    if (!m_cache->ReadOnlyOpen(progress)) {
        std::string message;
        bool isError = _error->PopMessage(message);
        if (isError)
            qWarning() << QString::fromStdString(message);

        m_trans->setError(QApt::InitError);
        m_trans->setErrorDetails(QString::fromStdString(message));

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

            string message;
            while(_error->PopMessage(message))
                m_trans->setErrorDetails(m_trans->errorDetails() +
                                         QString::fromStdString(message));
        }
    }

    // Clean up
    delete acquire;

    openCache(91, 95);
}

bool AptWorker::markChanges()
{
    pkgDepCache::ActionGroup *actionGroup = new pkgDepCache::ActionGroup(*m_cache);

    auto mapIter = m_trans->packages().constBegin();

    QApt::Package::State operation = QApt::Package::ToKeep;
    while (mapIter != m_trans->packages().constEnd()) {
        operation = (QApt::Package::State)mapIter.value().toInt();

        // Find package in cache
        pkgCache::PkgIterator iter;
        QString packageString = mapIter.key();
        QString version;

        // Check if a version is specified
        if (packageString.contains(QLatin1Char(','))) {
            QStringList split = packageString.split(QLatin1Char(','));
            iter = (*m_cache)->FindPkg(split.at(0).toStdString());
            version = split.at(1);
        } else {
            iter = (*m_cache)->FindPkg(packageString.toStdString());
        }

        // Check if the package was found
        if (iter == 0) {
            m_trans->setError(QApt::NotFoundError);
            m_trans->setErrorDetails(packageString);

            delete actionGroup;
            return false;
        }

        pkgDepCache::StateCache &State = (*m_cache)[iter];
        pkgProblemResolver resolver(*m_cache);
        bool toPurge = false;

        // Then mark according to the instruction
        switch (operation) {
        case QApt::Package::Held:
            (*m_cache)->MarkKeep(iter, false);
            (*m_cache)->SetReInstall(iter, false);
            resolver.Protect(iter);
            break;
        case QApt::Package::ToUpgrade: {
            bool fromUser = !(State.Flags & pkgCache::Flag::Auto);
            (*m_cache)->MarkInstall(iter, true, 0, fromUser);

            resolver.Clear(iter);
            resolver.Protect(iter);
            break;
        }
        case QApt::Package::ToInstall:
            (*m_cache)->MarkInstall(iter, true);

            resolver.Clear(iter);
            resolver.Protect(iter);
            break;
        case QApt::Package::ToReInstall:
            (*m_cache)->SetReInstall(iter, true);
            break;
        case QApt::Package::ToDowngrade: {
            pkgVersionMatch Match(version.toStdString(), pkgVersionMatch::Version);
            pkgCache::VerIterator Ver = Match.Find(iter);

            (*m_cache)->SetCandidateVersion(Ver);

            (*m_cache)->MarkInstall(iter, true);

            resolver.Clear(iter);
            resolver.Protect(iter);
            break;
        }
        case QApt::Package::ToPurge:
            toPurge = true;
        case QApt::Package::ToRemove:
            (*m_cache)->SetReInstall(iter, false);
            (*m_cache)->MarkDelete(iter, toPurge);

            resolver.Clear(iter);
            resolver.Protect(iter);
            resolver.Remove(iter);
        default:
            break;
        }
        mapIter++;
    }

    delete actionGroup;

    if (_error->PendingError() && ((*m_cache)->BrokenCount() == 0))
        _error->Discard(); // We had dep errors, but fixed them

    if (_error->PendingError())
    {
        // We've failed to mark the packages
        m_trans->setError(QApt::MarkingError);
        string message;
        if (_error->PopMessage(message))
            m_trans->setErrorDetails(QString::fromStdString(message));

        return false;
    }

    return true;
}

void AptWorker::upgradeSystem()
{
    if (m_trans->safeUpgrade())
        pkgAllUpgrade(*m_cache);
    else
        pkgDistUpgrade(*m_cache);

    commitChanges();
}

void AptWorker::commitChanges()
{
    // Initialize fetcher with our progress watcher
    WorkerAcquire *acquire = new WorkerAcquire(this, 15, 50);
    acquire->setTransaction(m_trans);

    pkgAcquire fetcher;
    fetcher.Setup(acquire);

    pkgPackageManager *packageManager;
    packageManager = _system->CreatePM(*m_cache);

    // Populate the fetcher with the needed archives
    if (!packageManager->GetArchives(&fetcher, m_cache->GetSourceList(), m_records) ||
        _error->PendingError()) {
        m_trans->setError(QApt::FetchError);
        delete acquire;
        return;
    }

    // Check for enough free space
    double FetchBytes = fetcher.FetchNeeded();
    double FetchPBytes = fetcher.PartialPresent();

    struct statvfs Buf;
    string OutputDir = _config->FindDir("Dir::Cache::Archives");
    if (statvfs(OutputDir.c_str(),&Buf) != 0) {
        m_trans->setError(QApt::DiskSpaceError);
        delete acquire;
        return;
    }

    if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize) {
        struct statfs Stat;
        if (statfs(OutputDir.c_str(), &Stat) != 0 ||
            unsigned(Stat.f_type)            != RAMFS_MAGIC)
        {
            m_trans->setError(QApt::DiskSpaceError);
            delete acquire;
            return;
        }
    }

    // Check for untrusted packages
    QStringList untrustedPackages;
    for (auto it = fetcher.ItemsBegin(); it < fetcher.ItemsEnd(); ++it) {
        if (!(*it)->IsTrusted())
            untrustedPackages << QString::fromStdString((*it)->ShortDesc());
    }

    if (!untrustedPackages.isEmpty()) {
        bool allowUntrusted = _config->FindB("APT::Get::AllowUnauthenticated", true);

        if (!allowUntrusted) {
            m_trans->setError(QApt::UntrustedError);

            delete acquire;
            return;
        }

        if (m_trans->frontendCaps() & QApt::UntrustedPromptCap) {
            m_trans->setUntrustedPackages(untrustedPackages, allowUntrusted);

            // Wait until the user approves, disapproves, or cancels the transaction
            while (m_trans->isPaused())
                 usleep(200000);
        }

        if (!m_trans->allowUntrusted()) {
            m_trans->setError(QApt::UntrustedError);

            delete acquire;
            return;
        }
    }

    // Fetch archives from the network
    if (fetcher.Run() != pkgAcquire::Continue) {
        // Our fetcher will report warnings for itself, but if it fails entirely
        // we have to send the error and finished signals
        if (!m_trans->isCancelled()) {
            m_trans->setError(QApt::FetchError);
        }

        delete acquire;
        return;
    }

    delete acquire;

    // Check for cancellation during fetch, or fetch errors
    if (m_trans->isCancelled())
        return;

    bool failed = false;
    for (auto i = fetcher.ItemsBegin(); i != fetcher.ItemsEnd(); ++i) {
        if((*i)->Status == pkgAcquire::Item::StatDone && (*i)->Complete)
            continue;
        if((*i)->Status == pkgAcquire::Item::StatIdle)
            continue;

        failed = true;
        break;
    }

    if (failed && !packageManager->FixMissing()) {
        m_trans->setError(QApt::FetchError);
        return;
    }

    // Set up the install
    WorkerInstallProgress installProgress(50, 90);
    installProgress.setTransaction(m_trans);
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

    pkgPackageManager::OrderResult res = installProgress.start(packageManager);
    bool success = (res == pkgPackageManager::Completed);

    // See how the installation went
    if (!success) {
        m_trans->setError(QApt::CommitError);
        // Error details set by WorkerInstallProgress
    }

    openCache(91, 95);
}

void AptWorker::downloadArchives()
{
    // Initialize fetcher with our progress watcher
    WorkerAcquire *acquire = new WorkerAcquire(this, 15, 100);
    acquire->setTransaction(m_trans);

    pkgAcquire fetcher;
    fetcher.Setup(acquire);

    pkgIndexFile *index;

    for (const QString &packageString : m_trans->packages().keys()) {
        pkgCache::PkgIterator iter = (*m_cache)->FindPkg(packageString.toStdString());

        if (!iter)
            continue; // Package not found

        pkgCache::VerIterator ver = (*m_cache)->GetCandidateVer(iter);

        if (!ver || !ver.Downloadable() || !ver.Arch())
            continue; // Virtual package or not downloadable or broken

        // Obtain package info
        pkgCache::VerFileIterator vf = ver.FileList();
        pkgRecords::Parser &rec = m_records->Lookup(ver.FileList());

        // Try to cross match against the source list
        if (!m_cache->GetSourceList()->FindIndex(vf.File(), index))
            continue;

        string fileName = rec.FileName();
        string MD5sum = rec.MD5Hash();

        if (fileName.empty()) {
            m_trans->setError(QApt::NotFoundError);
            m_trans->setErrorDetails(packageString);
            delete acquire;
            return;
        }

        new pkgAcqFile(&fetcher,
                       index->ArchiveURI(fileName),
                       MD5sum,
                       ver->Size,
                       index->ArchiveInfo(ver),
                       ver.ParentPkg().Name(),
                       m_trans->filePath().toStdString(), "");
    }

    if (fetcher.Run() != pkgAcquire::Continue) {
        // Our fetcher will report warnings for itself, but if it fails entirely
        // we have to send the error and finished signals
        if (!m_trans->isCancelled()) {
            m_trans->setError(QApt::FetchError);
        }
    }

    delete acquire;
    return;
}

void AptWorker::installFile()
{
    m_trans->setStatus(QApt::RunningStatus);

    // FIXME: bloody workaround.
    //        setTransaction contains logic WRT locale and debconf setup, which
    //        is also useful to dpkg. For now simply reuse WorkerInstallProgress
    //        as it has no adverse effects.
    WorkerInstallProgress installProgress(50, 90);
    installProgress.setTransaction(m_trans);

    QApt::DebFile deb(m_trans->filePath());

    QString debArch = deb.architecture();

    QStringList archList;
    archList.append(QLatin1String("all"));
    std::vector<std::string> archs = APT::Configuration::getArchitectures(false);

    for (std::string &arch : archs)
         archList.append(QString::fromStdString(arch));

    if (!archList.contains(debArch)) {
        m_trans->setError(QApt::WrongArchError);
        m_trans->setErrorDetails(debArch);
        return;
    }

    m_dpkgProcess = new QProcess(this);
    QString program = QLatin1String("dpkg") %
            QLatin1String(" -i ") % '"' % m_trans->filePath() % '"';
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    setenv("DEBIAN_FRONTEND", "passthrough", 1);
    setenv("DEBCONF_PIPE", "/tmp/qapt-sock", 1);
    m_dpkgProcess->start(program);
    connect(m_dpkgProcess, SIGNAL(started()), this, SLOT(dpkgStarted()));
    connect(m_dpkgProcess, SIGNAL(readyRead()), this, SLOT(updateDpkgProgress()));
    connect(m_dpkgProcess, SIGNAL(finished(int,QProcess::ExitStatus)),
            this, SLOT(dpkgFinished(int,QProcess::ExitStatus)));
}

void AptWorker::dpkgStarted()
{
    m_trans->setStatus(QApt::CommittingStatus);
}

void AptWorker::updateDpkgProgress()
{
    QString str = m_dpkgProcess->readLine();

    m_trans->setStatusDetails(str);
}

void AptWorker::dpkgFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        m_trans->setError(QApt::CommitError);
        m_trans->setErrorDetails(m_dpkgProcess->readAllStandardError());
    }

    m_dpkgProcess->deleteLater();
    m_dpkgProcess = nullptr;
}
