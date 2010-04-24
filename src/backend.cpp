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

#include "backend.h"

// Qt includes
#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusServiceWatcher>

// Apt includes
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/init.h>

namespace QApt {

class BackendPrivate
{
public:
    // The canonical list of all unique, non-virutal package objects
    Package::List packages;
    // List of unique indentifiers for the packages
    QList<int> packagesIndex;
    // Set of group names extracted from our packages
    QSet<Group*> groupSet;
};

Backend::Backend()
        : d(new BackendPrivate)
        , m_progressMeter()
        , m_cache(0)
        , m_policy(0)
        , m_depCache(0)
{
    m_list = new pkgSourceList;

    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "workerStarted", this, SLOT(workerStarted(const QString&)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "workerFinished", this, SLOT(workerFinished(const QString&, bool)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadProgress", this, SLOT(downloadProgress(int)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadMessage", this, SLOT(downloadMessage(int, const QString&)));

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(this);
    watcher->setConnection(QDBusConnection::systemBus());
    watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    watcher->addWatchedService("org.kubuntu.qaptworker");
    connect(watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            SLOT(serviceOwnerChanged(QString, QString, QString)));
}

Backend::~Backend()
{
    delete m_list;
    delete m_cache;
    delete m_policy;
    delete m_records;
    delete m_depCache;
}

bool Backend::init()
{
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

    pkgMakeStatusCache(*m_list, m_progressMeter, &m_map, true);
    m_progressMeter.Done();
    if (_error->PendingError()) {
        return false;
    }

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

    // Populate internal package cache
    int count = 0;
    QSet<QString> groups;

    pkgCache::PkgIterator iter;
    for (iter = m_depCache->PkgBegin(); iter.end() != true; iter++) {
        if (iter->VersionList == 0) {
            continue; // Exclude virtual packages.
        }

        Package *pkg = new Package(this, m_depCache, m_records, iter);
        // Here every unique package ID is given a value. By looking it up we
        // can get this count value...
        d->packagesIndex.insert(iter->ID, count);
        // ... and then retrieve a QApt::Package by looking at the value of the
        // key in the official Package::List, d->packages()
        d->packages.insert(count++, pkg);

        if (iter.Section()) {
            QString name = QString::fromStdString(iter.Section());
            groups << name;
        }
    }

    // Populate groups
    foreach (QString groupName, groups) {
        Group *group = new Group(this, groupName);
        d->groupSet << group;
    }

    return true;
}

bool Backend::reloadCache()
{
    if (init()) {
        return true;
    } else {
       return false;
    }
}

pkgSourceList *Backend::packageSourceList()
{
    return m_list;
}

Package *Backend::package(const QString &name)
{
    foreach (Package *package, d->packages) {
        if (package->name() == name) {
            return package;
        }
    }

    // FIXME: Need some type of fake package to return here if all else fails.
    // Otherwise, make sure you don't give this function invalid data. Sucks,
    // I know...
    qDebug() << "Porked!";
    return 0;
}

int Backend::packageCount()
{
    int packageCount = d->packages.size();

    return packageCount;
}

int Backend::packageCount(const Package::PackageStates &states)
{
    int packageCount = 0;

    foreach(Package *package, d->packages) {
        if ((package->state() & states)) {
            packageCount++;
        }
    }

    return packageCount;
}

Package::List Backend::availablePackages()
{
    return d->packages;
}

Package::List Backend::upgradeablePackages()
{
    Package::List upgradeablePackages;

    foreach (Package *package, d->packages) {
        if (package->state() & Package::Upgradeable) {
            upgradeablePackages << package;
        }
    }

    return upgradeablePackages;
}

Group *Backend::group(const QString &name)
{
    foreach (Group *group, d->groupSet) {
        if (group->name() == name) {
            return group;
        }
    }
}

Group::List Backend::availableGroups()
{
    Group::List groupList = d->groupSet.toList();

    return groupList;
}

void Backend::updateCache()
{
    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
              "/",
              "org.kubuntu.qaptworker",
              QLatin1String("updateCache"));
    QDBusConnection::systemBus().asyncCall(message);
}

void Backend::workerStarted(const QString &name)
{
    qDebug() << "Worker Started!";
    if (name == "update") {
        emit cacheUpdateStarted();
    }
}

void Backend::workerFinished(const QString &name, bool result)
{
    qDebug() << "Worker Finished!";
    if (name == "update") {
        emit cacheUpdateFinished();
    }
}

void Backend::cancelCacheUpdate()
{
    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
              "/",
              "org.kubuntu.qaptworker",
              QLatin1String("cancelCacheUpdate"));
    QDBusConnection::systemBus().asyncCall(message);
    qDebug() << "Canceling..";
}

void Backend::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    if (oldOwner.isEmpty()) {
        return; // Don't care, just appearing
    }

    if (newOwner.isEmpty()) {
        qDebug() << "It looks like our worker got lost";

        // Ok, something got screwed. Report and flee
        // emit errorOccurred((int) Aqpm::Globals::WorkerDisappeared, QVariantMap());
        // workerResult(false);
    }
}

}
