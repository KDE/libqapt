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

#ifndef QAPT_CACHE_H
#define QAPT_CACHE_H

#include <QtCore/QObject>

class pkgCache;
class pkgDepCache;
class pkgPolicy;
class pkgSourceList;

namespace QApt {

/**
 * CachePrivate is a class containing all private members of the Cache class
 */
class CachePrivate;

/**
 * The Cache class is what handles the internal APT package cache. If you are
 * using the Backend class, you will not need to worry about this class at all,
 * as it has its own Cache object and handles the opening/re-opening of the
 * internal APT cache when necessary.
 *
 * @author Jonathan Thomas
 */
class Cache : public QObject
{
    Q_OBJECT
public:
     /**
      * Default constructor
      */
    explicit Cache(QObject* parent);

     /**
      * Default destructor
      */
    virtual ~Cache();

    /**
     * This function returns a pointer to the interal dependency cache, which
     * keeps track of inter-package dependencies.
     *
     * @return A pointer to the internal @c pkgDepCache
     */
    pkgDepCache *depCache() const;

    /**
     * This function returns a pointer to the interal package source list,
     *
     * @return A pointer to the internal @c pkgSourceList
     */
    pkgSourceList *list() const;

public Q_SLOTS:
    /**
     * This function initializes the internal package cache. It is also used
     * to re-open the cache when the need arises. (E.g. such as an updated
     * sources list, or a package installation or removal)
     *
     * @return @c true if opening succeeds
     * @return @c false if opening fails
     */
    bool open();

protected:
    CachePrivate *const d_ptr;

private:
    Q_DECLARE_PRIVATE(Cache);
};

}

#endif
