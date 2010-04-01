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

#include <QTimer>
#include <QDebug>
#include <QFile>

#include <polkit-qt-1/polkitqt1-authority.h>

// Apt includes
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/init.h>
#include <apt-pkg/algorithms.h>

#include "workeracquire.h"

using namespace PolkitQt1;

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

   _system->Lock();
   m_locked = true;

   //FIXME: should depend on the result of _system->lock()
   return true;
}

void QAptWorker::unlock()
{
   if (!m_locked)
      return;

   _system->UnLock();
   m_locked = false;
}

bool QAptWorker::updateCache()
{
    WorkerAcquire acquireStatus;
    Authority::Result result;
    SystemBusNameSubject *subject;
    bool authorized;

    subject = new SystemBusNameSubject(message().service());

    result = Authority::instance()->checkAuthorizationSync("org.kubuntu.qaptworker.updateCache",
             subject , Authority::AllowUserInteraction);
    if (result == Authority::Yes) {
        qDebug() << message().service() << QString("Auth'd");
        bool authorized = true;
    } else {
        qDebug() << message().service() << QString("Auth phailure");
        return false;
    }

    if (authorized) {
        initializeApt();
        lock();
        unlock();

        // Lock the list directory
        FileFd Lock;
        if (_config->FindB("Debug::NoLocking", false) == false)
        {
            Lock.Fd(GetLock(_config->FindDir("Dir::State::Lists") + "lock"));
            if (_error->PendingError()) {
                return false;
        //   return _error->Error(_("Unable to lock the list directory"));
            }
        }

        // do the work
        if (_config->FindB("APT::Get::Download",true) == true) {
            ListUpdate(acquireStatus, *m_list);
        }

        return true;
    }
}
