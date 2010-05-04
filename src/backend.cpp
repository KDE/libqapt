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
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusServiceWatcher>

// Apt includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/cachefile.h>
#include <apt-pkg/init.h>

namespace QApt {

class BackendPrivate
{
public:
    // The canonical list of all unique, non-virutal package objects
    Package::List packages;
    // Set of group names extracted from our packages
    QSet<Group*> groups;
};

Backend::Backend()
        : d(new BackendPrivate)
        , m_cache(0)
        , m_records(0)
{
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "errorOccurred", this, SLOT(errorOccurred(int, const QVariantMap&)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "workerStarted", this, SLOT(workerStarted(const QString&)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "workerFinished", this, SLOT(workerFinished(const QString&, bool)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadProgress", this, SLOT(downloadProgress(int)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadMessage", this, SLOT(downloadMessage(int, const QString&)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "commitProgress", this, SLOT(commitProgress(const QString&, int)));

    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(this);
    watcher->setConnection(QDBusConnection::systemBus());
    watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    watcher->addWatchedService("org.kubuntu.qaptworker");
    connect(watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            SLOT(serviceOwnerChanged(QString, QString, QString)));
}

Backend::~Backend()
{
    delete m_cache;
    delete m_records;
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

    reloadCache();

    return true;
}

void Backend::reloadCache()
{
    delete m_cache;
    m_cache = new Cache(this);
    m_cache->open();

    pkgDepCache *depCache = m_cache->depCache();

    delete m_records;
    m_records = new pkgRecords(*depCache);

    foreach(Package *package, d->packages) {
        package->deleteLater();
    }
    d->packages.clear();

    foreach(Group *group, d->groups) {
        group->deleteLater();
    }
    d->groups.clear();

    // Populate internal package cache
    int count = 0;
    QSet<QString> groupSet;

    pkgCache::PkgIterator iter;
    for (iter = depCache->PkgBegin(); iter.end() != true; iter++) {
        if (iter->VersionList == 0) {
            continue; // Exclude virtual packages.
        }

        Package *pkg = new Package(this, depCache, m_records, iter);
        d->packages.insert(count++, pkg);

        if (iter.Section()) {
            QString name = QString::fromStdString(iter.Section());
            groupSet << name;
        }
    }

    // Populate groups
    foreach (const QString &groupName, groupSet) {
        Group *group = new Group(this, groupName);
        d->groups << group;
    }
}

pkgSourceList *Backend::packageSourceList()
{
    return m_cache->list();
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
            if (package->state() & Package::Held) {
                continue; // Don't count held packages
            }
            upgradeablePackages << package;
        }
    }

    return upgradeablePackages;
}

Group *Backend::group(const QString &name)
{
    foreach (Group *group, d->groups) {
        if (group->name() == name) {
            return group;
        }
    }
}

Group::List Backend::availableGroups()
{
    Group::List groupList = d->groups.toList();

    return groupList;
}

void Backend::markPackagesForUpgrade()
{
    // TODO: Should say something if there's an error?
    pkgAllUpgrade(*m_cache->depCache());
}

void Backend::markPackagesForDistUpgrade()
{
    // TODO: Should say something if there's an error?
    pkgDistUpgrade(*m_cache->depCache());
}

void Backend::markPackageForInstall(const QString &name)
{
    Package *pkg = package(name);
    pkg->setInstall();
}

void Backend::markPackageForRemoval(const QString &name, bool purge)
{
    Package *pkg = package(name);
    pkg->setRemove(purge);
}

void Backend::commitChanges()
{
    QMap<QString, QVariant> instructionList;

    foreach (Package *package, d->packages) {
        int flags = package->state();
        // Cannot have any of these flags simultaneously
        int status = flags & (Package::ToKeep |
                              Package::NewInstall |
                              Package::ToReInstall |
                              Package::ToUpgrade |
                              Package::ToDowngrade |
                              Package::ToRemove);

        switch (status) {
           case Package::ToKeep:
               if (flags & Package::Held) {
                   instructionList.insert(package->name(), Package::Held);
               }
               break;
           case Package::NewInstall:
               instructionList.insert(package->name(), Package::ToInstall);
               qDebug() << "Installing:" << package->name();
               break;
           case Package::ToReInstall:
               instructionList.insert(package->name(), Package::ToReInstall);
               break;
           case Package::ToUpgrade:
               instructionList.insert(package->name(), Package::ToUpgrade);
               qDebug() << "Upgrading:" << package->name();
               break;
           case Package::ToDowngrade:
               instructionList.insert(package->name(), Package::ToDowngrade);
               break;
           case Package::ToRemove:
               if(flags & Package::ToPurge) {
                   instructionList.insert(package->name(), Package::ToPurge);
               } else {
                   instructionList.insert(package->name(), Package::ToRemove);
               }
               break;
        }
    }

    qDebug() << instructionList;

    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
              "/",
              "org.kubuntu.qaptworker",
              QLatin1String("commitChanges"));

    QList<QVariant> args;
    args << QVariant(instructionList);
    message.setArguments(args);
    QDBusConnection::systemBus().asyncCall(message);
}

void Backend::packageChanged(Package *package)
{
    qDebug() << "A package changed!";
    emit packageChanged();
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
    if (name == "update") {
        qDebug() << "Cache Update Started!";
        emit cacheUpdateStarted();
    } else if (name == "commitChanges") {
        qDebug() << "Install/remove operation Started!";
        emit commitChangesStarted();
    }
}

void Backend::workerFinished(const QString &name, bool result)
{
    qDebug() << "Worker Finished!";
    if (name == "update") {
        reloadCache();
        emit cacheUpdateFinished();
    } else if (name == "commitChanges") {
        reloadCache();
        emit commitChangesFinished();
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
