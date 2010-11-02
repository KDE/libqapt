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

#include <QtCore/QCoreApplication>

#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/policy.h>

namespace QApt {

class CacheBuildProgress : public OpProgress
{
public:
    CacheBuildProgress(){};

    virtual void Update() {
        // Evil but Necessary, libapt-pkg not thread safe, afaict
        QCoreApplication::processEvents();
    }
};

class CachePrivate
{
public:
    CachePrivate()
        : mmap(0)
        , cache(0)
        , policy(0)
        , depCache(0)
        , list(new pkgSourceList)
        , trustCache(new QHash<pkgCache::PkgFileIterator, pkgIndexFile*>)
    {
    }

    virtual ~CachePrivate()
    {
        delete list;
        delete cache;
        delete policy;
        delete depCache;
        delete mmap;
        delete trustCache;
    }

    CacheBuildProgress m_progressMeter;
    MMap *mmap;

    pkgCache *cache;
    pkgPolicy *policy;

    pkgDepCache *depCache;
    pkgSourceList *list;

    QHash<pkgCache::PkgFileIterator, pkgIndexFile*> *trustCache;
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
    if (d->cache) {
        delete d->cache;
        d->cache = 0;
    }
    if (d->policy) {
        delete d->policy;
        d->policy = 0;
    }
    if (d->depCache) {
        delete d->depCache;
        d->depCache = 0;
    }
    if (d->mmap) {
        delete d->mmap;
        d->mmap = 0;
    }

    // Read the sources list
    if (!d->list->ReadMainList()) {
        return false;
    }

    pkgMakeStatusCache(*(d->list), d->m_progressMeter, &(d->mmap), true);
    d->m_progressMeter.Done();
    if (_error->PendingError()) {
        return false;
    }

    // Open the cache file
    d->cache = new pkgCache(d->mmap);
    d->policy = new pkgPolicy(d->cache);
    if (!ReadPinFile(*(d->policy)) || !ReadPinDir(*(d->policy))) {
        return false;
    }

    if (_error->PendingError()) {
        return false;
    }

    d->depCache = new pkgDepCache(d->cache, d->policy);
    d->depCache->Init(&(d->m_progressMeter));

    d->trustCache->clear();

    if (d->depCache->DelCount() != 0 || d->depCache->InstCount() != 0) {
         return false;
     }

    return true;
}

pkgDepCache *Cache::depCache() const
{
    Q_D(const Cache);

    return d->depCache;
}

pkgSourceList *Cache::list() const
{
    Q_D(const Cache);

    return d->list;
}

QHash<pkgCache::PkgFileIterator, pkgIndexFile*> *Cache::trustCache() const
{
    Q_D(const Cache);

    return d->trustCache;
}

}
