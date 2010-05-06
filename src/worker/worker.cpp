/***************************************************************************
 *   Copyright © 2010 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#include "worker.h"

#include "qaptworkeradaptor.h"

// QApt includes
#include <../globals.h>
#include <../package.h>

// Apt includes
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/init.h>
#include <apt-pkg/algorithms.h>

#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

#define RAMFS_MAGIC     0x858458f6

#include "qaptauthorization.h"
#include "workeracquire.h"
#include "workerinstallprogress.h"

QAptWorker::QAptWorker(int &argc, char **argv)
        : QCoreApplication(argc, argv)
        , m_cache(0)
        , m_records(0)
        , m_locked(false)
{
    new QaptworkerAdaptor(this);

    if (!QDBusConnection::systemBus().registerService("org.kubuntu.qaptworker")) {
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        return;
    }

    if (!QDBusConnection::systemBus().registerObject("/", this)) {
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        return;
    }

    QTimer::singleShot(60000, this, SLOT(quit()));
    initializeApt();

    m_acquireStatus = new WorkerAcquire;
    connect(m_acquireStatus, SIGNAL(downloadProgress(int)),
            this, SLOT(emitDownloadProgress(int)));
    connect(m_acquireStatus, SIGNAL(downloadMessage(int, const QString&)),
            this, SLOT(emitDownloadMessage(int, const QString&)));
}

QAptWorker::~QAptWorker()
{
    delete m_cache;
    delete m_records;
}

bool QAptWorker::initializeApt()
{
    if (!pkgInitConfig(*_config)) {
        return false;
    }

    _config->Set("Initialized", 1);

    if (!pkgInitSystem(*_config, _system)) {
        return false;
    }

    delete m_cache;
    m_cache = new QApt::Cache(this);
    m_cache->open();

    pkgDepCache *depCache = m_cache->depCache();

    delete m_records;
    m_records = new pkgRecords(*depCache);

    return true;
}

bool QAptWorker::lock()
{
   if (m_locked)
      return true;

   m_locked = _system->Lock();

   return m_locked;
}

void QAptWorker::unlock()
{
   if (!m_locked)
      return;

   _system->UnLock();
   m_locked = false;
}

void QAptWorker::updateCache()
{
    if (!QApt::Auth::authorize("org.kubuntu.qaptworker.updateCache", message().service())) {
        emit errorOccurred(QApt::Globals::AuthError, QVariantMap());
        return;
    }

    emit workerStarted();
    emit workerEvent(QApt::Globals::CacheUpdateStarted);
    // Lock the list directory
    FileFd Lock;
    if (!_config->FindB("Debug::NoLocking", false)) {
        Lock.Fd(GetLock(_config->FindDir("Dir::State::Lists") + "lock"));
        if (_error->PendingError()) {
            emit errorOccurred(QApt::Globals::LockError, QVariantMap());
            emit workerFinished(false);
        }
    }

    // do the work
    if (_config->FindB("APT::Get::Download",true) == true) {
        bool result = ListUpdate(*m_acquireStatus, *m_cache->list());
        emit workerFinished(result);
    }

    emit workerEvent(QApt::Globals::CacheUpdateFinished);
}

void QAptWorker::cancelDownload()
{
    m_acquireStatus->requestCancel();
    emit workerFinished(false);
}

void QAptWorker::commitChanges(QMap<QString, QVariant> instructionList)
{
    if (!QApt::Auth::authorize("org.kubuntu.qaptworker.commitChanges", message().service())) {
        emit errorOccurred(QApt::Globals::AuthError, QVariantMap());
        return;
    }

    // Parse out the argument list and mark packages for operations
    QMap<QString, QVariant>::const_iterator mapIter = instructionList.constBegin();

    while (mapIter != instructionList.constEnd()) {
        QString package = mapIter.key();
        int operation = mapIter.value().toInt();

        // Iterate through all packages
        pkgCache::PkgIterator iter;
        for (iter = m_cache->depCache()->PkgBegin(); iter.end() != true; iter++) {
            // Find one with a matching Name to the one in the instructions list
            if (iter.Name() == package) {
                // Then mark according to the instruction
                if (operation == QApt::Package::Held) {
                    m_cache->depCache()->MarkKeep(iter, false);
                    m_cache->depCache()->SetReInstall(iter, false);
                } else if (operation == QApt::Package::ToInstall) {
                    m_cache->depCache()->MarkInstall(iter, true);
                    pkgDepCache::StateCache & State = (*m_cache->depCache())[iter];
                    if (!State.Install() || m_cache->depCache()->BrokenCount() > 0) {
                        pkgProblemResolver Fix(m_cache->depCache());
                        Fix.Clear(iter);
                        Fix.Protect(iter);
                        Fix.Resolve(true);
                    }
                } else if (operation == QApt::Package::ToReInstall) {
                    m_cache->depCache()->SetReInstall(iter, true);
                } else if (operation == QApt::Package::ToUpgrade) {
                    // The QApt Backend will handle dist-upgradish things for us
                    pkgAllUpgrade((*m_cache->depCache()));
                } else if (operation == QApt::Package::ToDowngrade) {
                    // TODO: Probably gotta set the candidate version here...
                    // needs work in QApt::Package so that we can set this anyways
                } else if (operation == QApt::Package::ToPurge) {
                    m_cache->depCache()->SetReInstall(iter, false);
                    m_cache->depCache()->MarkDelete(iter, true);
                } else if (operation == QApt::Package::ToRemove) {
                    m_cache->depCache()->SetReInstall(iter, false);
                    m_cache->depCache()->MarkDelete(iter, false);
                } else {
                    // Unsupported operation. Should emit something here?
                }
            }
        }
        mapIter++;
    }

    emit workerStarted();
    emit workerEvent(QApt::Globals::PackageDownloadStarted);

    // Lock the archive directory
    FileFd Lock;
    if (_config->FindB("Debug::NoLocking",false) == false)
    {
        Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
        if (_error->PendingError() == true) {
            emit errorOccurred(QApt::Globals::LockError, QVariantMap());
            emit workerEvent(QApt::Globals::PackageDownloadFinished);
            emit workerFinished(false);
            return;
        }
    }

    pkgAcquire fetcher(m_acquireStatus);

    pkgPackageManager *packageManager;
    packageManager = _system->CreatePM(m_cache->depCache());

    if (!packageManager->GetArchives(&fetcher, m_cache->list(), m_records) ||
        _error->PendingError()) {
        // WorkerAcquire emits its own error messages; just end the operation
        emit workerEvent(QApt::Globals::PackageDownloadFinished);
        emit workerFinished(false);
        return;
    }

    // Display statistics
    double FetchBytes = fetcher.FetchNeeded();
    double FetchPBytes = fetcher.PartialPresent();
    double DebBytes = fetcher.TotalNeeded();
    if (DebBytes != m_cache->depCache()->DebSize())
    {
        //TODO: emit mismatch warning. Need to decide on a warning signal.
    }

    /* Check for enough free space */
    struct statvfs Buf;
    string OutputDir = _config->FindDir("Dir::Cache::Archives");
    if (statvfs(OutputDir.c_str(),&Buf) != 0) {
        QVariantMap args;
        args["DirectoryString"] = QString::fromStdString(OutputDir.c_str());
        emit errorOccurred(QApt::Globals::DiskSpaceError, args);
        emit workerFinished(false);
        return;
    }
    if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
    {
        struct statfs Stat;
        if (statfs(OutputDir.c_str(), &Stat) != 0 ||
            unsigned(Stat.f_type)            != RAMFS_MAGIC)
        {
            QVariantMap args;
            args["DirectoryString"] = QString::fromStdString(OutputDir.c_str());
            emit errorOccurred(QApt::Globals::DiskSpaceError, args);
            emit workerFinished(false);
            return;
        }
    }

    if (fetcher.Run() != pkgAcquire::Continue) {
        // Our fetcher will report errors for itself, but we have to send the
        // finished signal
        emit workerEvent(QApt::Globals::PackageDownloadFinished);
        emit workerFinished(false);
        return;
    }

    emit workerEvent(QApt::Globals::PackageDownloadFinished);
    emit workerEvent(QApt::Globals::CommitChangesStarted);

    WorkerInstallProgress *installProgress = new WorkerInstallProgress(this);
    connect(installProgress, SIGNAL(commitError(int, const QVariantMap&)),
            this, SLOT(emitErrorOccurred(int, const QVariantMap&)));
    connect(installProgress, SIGNAL(commitProgress(const QString&, int)),
            this, SLOT(emitCommitProgress(const QString&, int)));

    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

    pkgPackageManager::OrderResult res = installProgress->start(packageManager);
    bool success = (res == pkgPackageManager::Completed);
    emit workerEvent(QApt::Globals::CommitChangesFinished);
    emit workerFinished(success);
}

void QAptWorker::emitDownloadProgress(int percentage)
{
    emit downloadProgress(percentage);
}

void QAptWorker::emitDownloadMessage(int flag, const QString& message)
{
    emit downloadMessage(flag, message);
}

void QAptWorker::emitCommitProgress(const QString& status, int percentage)
{
    emit commitProgress(status, percentage);
}

void QAptWorker::emitErrorOccurred(int code, const QVariantMap& details)
{
    emit errorOccurred(code, details);
}
