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

#include <QDebug>
#include <QFile>

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
        qDebug() << QDBusConnection::systemBus().lastError().message();
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        return;
    }

    if (!QDBusConnection::systemBus().registerObject("/", this)) {
        qDebug() << "unable to register service interface to dbus";
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
        return;
    }

    emit workerStarted("update");
    // Lock the list directory
    FileFd Lock;
    if (!_config->FindB("Debug::NoLocking", false)) {
        Lock.Fd(GetLock(_config->FindDir("Dir::State::Lists") + "lock"));
        if (_error->PendingError()) {
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
        return;
    }
    m_acquireStatus->requestCancel();
}

void QAptWorker::commitChanges(QMap<QString, QVariant> instructionList)
{
    // Parse out the argument list and mark packages for operations
    QMap<QString, QVariant>::const_iterator mapIter = instructionList.constBegin();

    while (mapIter != instructionList.constEnd()) {
        QString operation = mapIter.key();
        int packageID = mapIter.value().toInt();

        // Iterate through all packages
        pkgCache::PkgIterator iter;
        for (iter = m_depCache->PkgBegin(); iter.end() != true; iter++) {
            // Find one with a matching ID to the one in the instructions list
            if (iter->ID == packageID) {
                // Then mark according to the instruction
                if (operation == "kept") {
                    m_depCache->MarkKeep(iter, false);
                    m_depCache->SetReInstall(iter, false);
                } else if (operation == "toInstall") {
                    m_depCache->MarkInstall(iter, true);
                } else if (operation == "toReInstall") {
                    m_depCache->SetReInstall(iter, true);
                } else if (operation == "toUpgrade") {
                    // The QApt Backend will handle dist-upgradish things for us
                    pkgAllUpgrade((*m_depCache));
                } else if (operation == "toDowngrade") {
                    // TODO: Probably gotta set the candidate version here...
                    // needs work in QApt::Package so that we can set this anyways
                } else if (operation == "toPurge") {
                    m_depCache->SetReInstall(iter, false);
                    m_depCache->MarkDelete(iter, true);
                } else if (operation == "toRemove") {
                    m_depCache->SetReInstall(iter, false);
                    m_depCache->MarkDelete(iter, false);
                } else {
                    // Unsupported operation
                    // TODO: Probably should emit something here
                }
            }
        }
        mapIter++;
    }

    if (!QApt::Auth::authorize("org.kubuntu.qaptworker.commitChanges", message().service())) {
        return;
    }

    emit workerStarted("commitChanges");

    // Lock the archive directory
    FileFd Lock;
    if (_config->FindB("Debug::NoLocking",false) == false)
    {
        Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
        if (_error->PendingError() == true) {
            // TODO: Send error about not being able to lock
            return;
        }
    }

    pkgAcquire fetcher(m_acquireStatus);

    pkgPackageManager *packageManager;
    packageManager = _system->CreatePM(m_depCache);

    WorkerInstallProgress *installProgress = new WorkerInstallProgress(this);
    connect(installProgress, SIGNAL(transactionProgress(const QString&, const QString&, int)),
            this, SLOT(emitTransactionProgress(const QString&, const QString&, int)));

    if (!packageManager->GetArchives(&fetcher, m_list, m_records) ||
        _error->PendingError()) {
        //TODO: Notify of error
        return;
    }

    // Display statistics
    double FetchBytes = fetcher.FetchNeeded();
    double FetchPBytes = fetcher.PartialPresent();
    double DebBytes = fetcher.TotalNeeded();
    if (DebBytes != m_depCache->DebSize())
    {
        //TODO: emit mismatch warning
    }

    /* Check for enough free space */
    struct statvfs Buf;
    string OutputDir = _config->FindDir("Dir::Cache::Archives");
    if (statvfs(OutputDir.c_str(),&Buf) != 0) {
        //TODO: emit "can't calculate free space" error
        return;
    }
    if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
    {
        struct statfs Stat;
        if (statfs(OutputDir.c_str(), &Stat) != 0 ||
            unsigned(Stat.f_type)            != RAMFS_MAGIC)
        {
//             return _error->Error("You don't have enough free space in %s.",
//                         OutputDir.c_str());
            // TODO: emit space error
            return;
        }
    }

    if (fetcher.Run() != pkgAcquire::Continue) {
        //TODO: Notify of fetching error
        return;
    }

    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

    pkgPackageManager::OrderResult res = installProgress->start(packageManager);
    if (res == pkgPackageManager::Completed) {
        emit workerFinished("commitChanges", true);
    } else {
        //TODO: emit failure
    }
}

void QAptWorker::emitDownloadProgress(int percentage)
{
    emit downloadProgress(percentage);
}

void QAptWorker::emitDownloadMessage(int flag, const QString& message)
{
    emit downloadMessage(flag, message);
}

void QAptWorker::emitTransactionProgress(const QString& package, const QString& status, int percentage)
{
    emit transactionProgress(package, status, percentage);
}
