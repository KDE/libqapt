/***************************************************************************
 *   Copyright © 2014 Harald Sitter <sitter@kde.org>                       *
 *   Copyright © 2011 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#ifndef QAPT_DEPENDENCYINFO_H
#define QAPT_DEPENDENCYINFO_H

#include <QSharedDataPointer>
#include <QString>

#include "globals.h"

namespace QApt {

class DependencyInfoPrivate;

/**
 * The DependencyInfo object describes a single dependency that a package can
 * have. DependencyInfo objects that are part of a group of "or" dependencies
 * will return @c true for the isOrDependency() function
 *
 * This class has a rather low-level dependency. You will most likely not need
 * to use this function, unless you are installing packages via .deb files
 * directly with dpkg circumventing APT, and wish to check that all
 * dependencies are satisfied before blindly installing the package with dpkg.
 *
 * @since 1.2
 *
 * @author Jonathan Thomas
 */
class Q_DECL_EXPORT DependencyInfo
{
public:
   /**
    * Default constructor.
    *
    * All unspecified fields are either empty or 0
    */
    DependencyInfo();

   /**
    * Copy constructor. Creates a shallow copy.
    */
    DependencyInfo(const DependencyInfo &other);

   /**
    * Default destructor.
    */
    ~DependencyInfo();

   /**
    * Assignment operator.
    */
    DependencyInfo &operator=(const DependencyInfo &rhs);

    static QList<QList<DependencyInfo> > parseDepends(const QString &field, DependencyType type);

   /**
    * The name of the package that the dependency describes.
    *
    * @return The package name of the dependency
    */
    QString packageName() const;

   /**
    * The version of the package that the dependency describes.
    *
    * @return The package version of the dependency
    */
    QString packageVersion() const;

   /**
    * The logical relation in regards to the version that the dependency
    * describes
    *
    * @return The logical relation of the dependency
    */
    RelationType relationType() const;

   /**
    * The dependency type that the dependency describes, such as a
    * "Depend", "Recommends", "Suggests", etc.
    *
    * @return The @c DependencyType of the dependency
    */
    DependencyType dependencyType() const;

    /**
     * @return the multi-arch annotation defining on whether Multi-Arch: allowed
     *         dependency candidates may be used to resolve the dependency.
     *         A non-empty annotation would be either 'any', 'native' or a
     *         specific architecture tag (please refer to the multi-arch specifiation
     *         for additional information on what the tags mean).
     */
    QString multiArchAnnotation() const;

    /**
     * @return a localized string representation of the type.
     */
    static QString typeName(DependencyType type);

private:
    DependencyInfo(const QString &package,
                   const QString &version,
                   RelationType rType,
                   DependencyType dType);

    QSharedDataPointer<DependencyInfoPrivate> d;

    friend class Package;
};

/**
 * A DependencyItem is a list of DependencyInfo objects that are all
 * interchangable "or" dependencies of a package. Any one of the package
 * name/version combinations specified by the DependencyInfo objects in this
 * list will satisfy the dependency
 */
typedef QList<DependencyInfo> DependencyItem;

}

Q_DECLARE_TYPEINFO(QApt::DependencyInfo, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QList<QApt::DependencyInfo>, Q_MOVABLE_TYPE);

#endif
