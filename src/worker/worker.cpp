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
#include <../backend.h>

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
        , m_progressMeter()
        , m_cache(0)
        , m_policy(0)
        , m_depCache(0)
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

bool QAptWorker::initializeApt()
{
    m_list = new pkgSourceList;

    if (!pkgInitConfig(*_config)) {
        return false;
    }

    _config->Set("Initialized", 1);

    if (!pkgInitSystem(*_config, _system)) {
        return false;
    }

   // delete any old structures
    if(m_depCache)
        delete m_depCache;
    if(m_policy)
        delete m_policy;
    if(m_cache)
        delete m_cache;

    // Read the sources list
    if (!m_list->ReadMainList()) {
        return false;
    }

    pkgMakeStatusCache(*m_list, m_progressMeter, 0, true);
    m_progressMeter.Done();

    // Open the cache file
    FileFd File;
    File.Open(_config->FindFile("Dir::Cache::pkgcache"), FileFd::ReadOnly);
    if (_error->PendingError()) {
        return false;
    }

   m_map = new MMap(File, MMap::Public | MMap::ReadOnly);
   if (_error->PendingError())
      return false;

    // Open the cache file
    m_cache = new pkgCache(m_map);
    m_policy = new pkgPolicy(m_cache);
    m_records = new pkgRecords(*m_cache);
    if (!ReadPinFile(*m_policy)) {
        return false;
    }

    if (_error->PendingError()) {
        return false;
    }

    m_depCache = new pkgDepCache(m_cache, m_policy);
    m_depCache->Init(&m_progressMeter);

    if (m_depCache->DelCount() != 0 || m_depCache->InstCount() != 0) {
        return false;
    }

    return true;
}

QAptWorker::~QAptWorker()
{
    delete m_list;
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

    emit workerStarted("update");
    // Lock the list directory
    FileFd Lock;
    if (!_config->FindB("Debug::NoLocking", false)) {
        Lock.Fd(GetLock(_config->FindDir("Dir::State::Lists") + "lock"));
        if (_error->PendingError()) {
            emit errorOccurred(QApt::Globals::LockError, QVariantMap());
            emit workerFinished("update", false);
        }
    }

    // do the work
    if (_config->FindB("APT::Get::Download",true) == true) {
        bool result = ListUpdate(*m_acquireStatus, *m_list);
        emit workerFinished("update", result);
    }
}

void QAptWorker::cancelCacheUpdate()
{
    if (!QApt::Auth::authorize("org.kubuntu.qaptworker.updateCache", message().service())) {
        emit errorOccurred(QApt::Globals::AuthError, QVariantMap());
        emit workerFinished("update", false);
        return;
    }
    m_acquireStatus->requestCancel();
    emit workerFinished("update", false);
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
        for (iter = m_depCache->PkgBegin(); iter.end() != true; iter++) {
            // Find one with a matching Name to the one in the instructions list
            if (iter.Name() == package) {
                // Then mark according to the instruction
                if (operation == QApt::Package::Held) {
                    m_depCache->MarkKeep(iter, false);
                    m_depCache->SetReInstall(iter, false);
                } else if (operation == QApt::Package::ToInstall) {
                    m_depCache->MarkInstall(iter, true);
                    pkgDepCache::StateCache & State = (*m_depCache)[iter];
                    if (!State.Install() || m_depCache->BrokenCount() > 0) {
                        pkgProblemResolver Fix(m_depCache);
                        Fix.Clear(iter);
                        Fix.Protect(iter);
                        Fix.Resolve(true);
                    }
                } else if (operation == QApt::Package::ToReInstall) {
                    m_depCache->SetReInstall(iter, true);
                } else if (operation == QApt::Package::ToUpgrade) {
                    // The QApt Backend will handle dist-upgradish things for us
                    pkgAllUpgrade((*m_depCache));
                } else if (operation == QApt::Package::ToDowngrade) {
                    // TODO: Probably gotta set the candidate version here...
                    // needs work in QApt::Package so that we can set this anyways
                } else if (operation == QApt::Package::ToPurge) {
                    m_depCache->SetReInstall(iter, false);
                    m_depCache->MarkDelete(iter, true);
                } else if (operation == QApt::Package::ToRemove) {
                    m_depCache->SetReInstall(iter, false);
                    m_depCache->MarkDelete(iter, false);
                } else {
                    // Unsupported operation. Should emit something here?
                }
            }
        }
        mapIter++;
    }

    emit workerStarted("commitChanges");

    // Lock the archive directory
    FileFd Lock;
    if (_config->FindB("Debug::NoLocking",false) == false)
    {
        Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
        if (_error->PendingError() == true) {
            emit errorOccurred(QApt::Globals::LockError, QVariantMap());
            emit workerFinished("commitChanges", false);
            return;
        }
    }

    pkgAcquire fetcher(m_acquireStatus);

    pkgPackageManager *packageManager;
    packageManager = _system->CreatePM(m_depCache);

    WorkerInstallProgress *installProgress = new WorkerInstallProgress(this);
    connect(installProgress, SIGNAL(commitError(int, const QVariantMap&)),
            this, SLOT(emitErrorOccurred(int, const QVariantMap&)));
    connect(installProgress, SIGNAL(transactionProgress(const QString&, int)),
            this, SLOT(emitTransactionProgress(const QString&, int)));

    if (!packageManager->GetArchives(&fetcher, m_list, m_records) ||
        _error->PendingError()) {
        // WorkerAcquire emits its own error messages; just end the operation
        emit workerFinished("commitChanges", false);
        return;
    }

    // Display statistics
    double FetchBytes = fetcher.FetchNeeded();
    double FetchPBytes = fetcher.PartialPresent();
    double DebBytes = fetcher.TotalNeeded();
    if (DebBytes != m_depCache->DebSize())
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
        emit workerFinished("commitChanges", false);
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
            emit workerFinished("commitChanges", false);
            return;
        }
    }

    if (fetcher.Run() != pkgAcquire::Continue) {
        // Our fetcher will report errors for itself, but we have to send the
        // finished signal
        emit workerFinished("commitChanges", false);
        return;
    }

    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

    pkgPackageManager::OrderResult res = installProgress->start(packageManager);
    bool success = (res == pkgPackageManager::Completed);
    emit workerFinished("commitChanges", success);
}

void QAptWorker::emitDownloadProgress(int percentage)
{
    emit downloadProgress(percentage);
}

void QAptWorker::emitDownloadMessage(int flag, const QString& message)
{
    emit downloadMessage(flag, message);
}

void QAptWorker::emitTransactionProgress(const QString& status, int percentage)
{
    emit transactionProgress(status, percentage);
}

void QAptWorker::emitErrorOccurred(int code, const QVariantMap& details)
{
    emit errorOccurred(code, details);
}
