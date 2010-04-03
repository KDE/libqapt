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

#ifndef GROUP_H
#define GROUP_H

#include <QtCore/QObject>

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>

#include "package.h"

/**
 * The QApt namespace is the main namespace for LibQApt. All classes in this
 * library fall under this namespace.
 */
namespace QApt {

/**
 * GroupPrivate is a class containing all private members of the Group class
 */
class GroupPrivate;

/**
 * The group class is a container for package groups, which are defined as
 * "sections" in the libapt-pkg API.
 *
 * @author Jonathan Thomas
 */
class Group : public QObject
{
    Q_OBJECT
public:
   /**
    * Default constructor
    */
    Group(QObject* parent, const QString &name, pkgCache *cache,
          pkgDepCache *depCache, pkgRecords *records);

   /**
    * Default destructor
    */
    virtual ~Group();

   /**
    * Defines the Group::List type, which is a QList of Groups
    */
    typedef QList<Group*> List;

   /**
    * Pointer to the Apt package cache passed to us by the constructor
    */
    pkgCache *m_cache;

   /**
    * Pointer to the Apt dependency cache passed to us by the constructor
    */
    pkgDepCache *m_depCache;

   /**
    * Pointer to the Apt package records passed to us by the constructor
    */
    pkgRecords *m_records;


    /**
     * Member function that returns the name of the group
     *
     * \return The name of the group as a @c QString
     */
    QString name() const;

    /**
     * A list of all packages in the group
     *
     * \return A @c Package::List of all packages in the group
     */
    Package::List packages();

private:
    /**
     * Pointer to the GroupPrivate class that contains all of Group's private
     * members
     */
    GroupPrivate * const d;

};

}

#endif
