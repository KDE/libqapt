/***************************************************************************
 *   Copyright © 2010 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *   Heavily inspired by Synaptic library code ;-)                         *
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

#include "group.h"

namespace QApt {

class GroupPrivate
{
    public:
        QString name;
};

Group::Group(QObject* parent, const QString &name, pkgCache *cache,
             pkgDepCache *depCache, pkgRecords *records)
        : QObject(parent)
        , d(new GroupPrivate)
{
    d->name = name;

    m_cache = cache;
    m_depCache = depCache;
    m_records = records;
}

Group::~Group()
{
    delete d;
}

QString Group::name() const
{
    return d->name;
}

Package::List Group::packages()
{
    Package::List packages;

    pkgCache::PkgIterator it = m_cache->PkgBegin();
    for(;it!=m_cache->PkgEnd();++it) {
        if (it.Section() == d->name) {
          Package *package = new Package(this, m_depCache, m_records, it);
          packages << package;
        }
    }

    return packages;
}

}