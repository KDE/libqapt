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

// Apt includes
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/init.h>

namespace QApt {

Backend::Backend()
        : m_progressMeter()
        , m_cache(0)
        , m_policy(0)
        , m_depCache(0)
{
    m_list = new pkgSourceList;
}

Backend::~Backend()
{
    delete m_list;
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

Package *Backend::package(const QString &name)
{
    pkgCache::PkgIterator it = m_cache->PkgBegin();
    for(;it!=m_cache->PkgEnd();++it) {
        if (it.Name() == name) {
            Package *package = new Package(this, m_depCache, m_records, it);
            return package;
        }
    }

    // FIXME: Need some type of fake package to return here if all else fails.
    // Otherwise, make sure you don't give this function invalid data. Sucks,
    // I know...
    qDebug() << "Porked!";
}

int Backend::packageCount()
{
    int packageCount = 0;

    pkgCache::PkgIterator it = m_cache->PkgBegin();
    for(;it!=m_cache->PkgEnd();++it) {
        pkgDepCache::StateCache & state = (*m_depCache)[it];
        // Don't count no-longer-existant packages
        if (!state.CandidateVer == 0) {
            packageCount++;
        }
    }

    return packageCount;
}

int Backend::packageCount(const Package::PackageStates &states)
{
    int packageCount = 0;

    pkgCache::PkgIterator it = m_cache->PkgBegin();
    for(;it!=m_cache->PkgEnd();++it) {
        Package *package = new Package(this, m_depCache, m_records, it);
        if ((package->state() & states)) {
            packageCount++;
        }
    }

    return packageCount;
}

Package::List Backend::availablePackages()
{
    Package::List availablePackages;

    pkgCache::PkgIterator it = m_cache->PkgBegin();
    for(;it!=m_cache->PkgEnd();++it) {
        Package *package = new Package(this, m_depCache, m_records, it);
        availablePackages << package;
    }

    return availablePackages;
}

Package::List Backend::upgradeablePackages()
{
    Package::List upgradeablePackages;

    foreach (Package *package, availablePackages()) {
        if (package->state() & Package::Upgradeable) {
            upgradeablePackages << package;
        }
    }

    return upgradeablePackages;
}

Group *Backend::group(const QString &name)
{
    Group *group = new Group(this, name, m_cache, m_depCache, m_records);
    return group;
}

Group::List Backend::availableGroups()
{
    QStringList groupStringList;
    pkgCache::PkgIterator it = m_cache->PkgBegin();
    for(;it!=m_cache->PkgEnd();++it)
    {
        groupStringList << it.Section();
    }

    QSet<QString> groupSet = groupStringList.toSet();

    Group::List groupList;

    foreach(const QString &name, groupSet) {
        Group *group = new Group(this, name, m_cache, m_depCache, m_records);
        groupList << group;
    }

    return groupList;
}

bool Backend::updateCache()
{
    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
              "/",
              "org.kubuntu.qaptworker",
              QLatin1String("updateCache"));
    // notice the systemBus here..
    QDBusMessage reply = QDBusConnection::systemBus().call(message);
    qDebug() << QDBusConnection::systemBus().lastError().message();
    if (reply.type() == QDBusMessage::ReplyMessage
        && reply.arguments().size() == 1) {
        // the reply can be anything, here we receive a bool
        if (reply.arguments().first().toBool()) {
            return true;
        }
    }

    return false;
}

}
