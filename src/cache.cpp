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

#include <apt-pkg/cachefile.h>

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
        : cache(0)
        , trustCache(new QHash<pkgCache::PkgFileIterator, pkgIndexFile*>)
    {
    }

    virtual ~CachePrivate()
    {
        delete cache;
        delete trustCache;
    }

    CacheBuildProgress progressMeter;
    pkgCacheFile *cache;

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
    delete d->cache;
    d->cache = 0;

    bool shouldLock = (geteuid == 0);

    d->cache = new pkgCacheFile();

    // Build the cache
    if (!d->cache->Open(&d->progressMeter, shouldLock)) {
        return false;
    }

    d->trustCache->clear();

    return true;
}

pkgDepCache *Cache::depCache() const
{
    Q_D(const Cache);

    return *d->cache;
}

pkgSourceList *Cache::list() const
{
    Q_D(const Cache);

    return d->cache->GetSourceList();
}

QHash<pkgCache::PkgFileIterator, pkgIndexFile*> *Cache::trustCache() const
{
    Q_D(const Cache);

    return d->trustCache;
}

}
