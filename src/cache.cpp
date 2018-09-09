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

#include <QCoreApplication>

#include <apt-pkg/cachefile.h>

namespace QApt {

class CachePrivate
{
public:
    CachePrivate()
        : cache(new pkgCacheFile())
        , trustCache(new QHash<pkgCache::PkgFileIterator, pkgIndexFile*>)
    {
    }

    ~CachePrivate()
    {
        delete cache;
        delete trustCache;
    }

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

    // Close cache in case it's been opened
    d->cache->Close();
    d->trustCache->clear();

    // Build the cache, return whether it opened
    return d->cache->ReadOnlyOpen();
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
