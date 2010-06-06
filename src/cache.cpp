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

#include "cache.h"

#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>


namespace QApt {

class CachePrivate
{
public:
    CachePrivate()
        : m_map(0)
        , m_cache(0)
        , m_policy(0)
        , m_depCache(0)
        , m_list(new pkgSourceList)
    {
    }

    virtual ~CachePrivate()
    {
        delete m_list;
        delete m_cache;
        delete m_policy;
        delete m_depCache;
        delete m_map;
    }

    OpProgress m_progressMeter;
    MMap *m_map;

    pkgCache *m_cache;
    pkgPolicy *m_policy;

    pkgDepCache *m_depCache;
    pkgSourceList *m_list;
};

Cache::Cache(QObject* parent)
        : QObject(parent)
        , d_ptr(new CachePrivate)
{
}

Cache::~Cache()
{
    delete d_ptr;
}

bool Cache::open()
{
    Q_D(Cache);

   // delete any old structures
    delete d->m_cache;
    delete d->m_policy;
    delete d->m_depCache;
    delete d->m_map;

    // Read the sources list
    if (!d->m_list->ReadMainList()) {
        return false;
    }

    pkgMakeStatusCache(*(d->m_list), d->m_progressMeter, &(d->m_map), true);
    d->m_progressMeter.Done();
    if (_error->PendingError()) {
        return false;
    }

    // Open the cache file
    d->m_cache = new pkgCache(d->m_map);
    d->m_policy = new pkgPolicy(d->m_cache);
    if (!ReadPinFile(*(d->m_policy))) {
        return false;
    }

    if (_error->PendingError()) {
        return false;
    }

    d->m_depCache = new pkgDepCache(d->m_cache, d->m_policy);
    d->m_depCache->Init(&(d->m_progressMeter));

    if (d->m_depCache->DelCount() != 0 || d->m_depCache->InstCount() != 0) {
        return false;
    }
}

pkgDepCache *Cache::depCache()
{
    Q_D(Cache);

    return d->m_depCache;
}

pkgSourceList *Cache::list()
{
    Q_D(Cache);

    return d->m_list;
}

}
