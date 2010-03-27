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
}

bool Backend::init()
{
    if (_config->FindB("Initialized"))
        return true;

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

Package *Backend::package(const QString &name)
{
    pkgCache::PkgIterator it = m_cache->PkgBegin();
    for(;it!=m_cache->PkgEnd();++it)
    {
        if (it.Name() == name) {
            Package *package = new Package(this, m_depCache, m_records, it);
            return package;
        }
    }
}


}
