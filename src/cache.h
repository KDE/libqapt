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

#ifndef CACHE_H
#define CACHE_H

#include <QtCore/QObject>

#include <apt-pkg/depcache.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/pkgsystem.h>

class pkgCache;
class pkgPolicy;

/**
 * The QApt namespace is the main namespace for LibQApt. All classes in this
 * library fall under this namespace.
 */
namespace QApt {

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

    OpProgress m_progressMeter;
    MMap *m_map;

    pkgCache *m_cache;
    pkgPolicy *m_policy;

    pkgDepCache *m_depCache;
    pkgSourceList *m_list;

public Q_SLOTS:
    bool open();
    pkgDepCache *depCache();
    pkgSourceList *list();
};

}

#endif
