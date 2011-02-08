/***************************************************************************
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

#include "dependencyinfo.h"

#include <apt-pkg/deblistparser.h>

// Own includes
#include "package.h"

namespace QApt {

DependencyInfo::DependencyInfo()
    : m_relationType(NoOperand)
    , m_dependencyType(InvalidType)
{
}

DependencyInfo::DependencyInfo(const QString &package, const QString &version,
                               RelationType rType, DependencyType dType)
    : m_packageName(package)
    , m_packageVersion(version)
    , m_relationType(rType)
    , m_dependencyType(dType)
{
}

DependencyInfo::~DependencyInfo()
{
}

QList<DependencyItem> DependencyInfo::parseDepends(const QString &field, DependencyType type)
{
    string package;
    string version;
    unsigned int op;

    string fieldStr = field.toStdString();

    const char *start = fieldStr.c_str();
    const char *stop = start + strlen(start);

    QList<DependencyItem> depends;

    bool hadOr = false;
    while (start != stop) {
        DependencyItem depItem;

        start = debListParser::ParseDepends(start, stop, package, version, op,
                                            false);

        if (!start) {
            // Parsing error
            return depends;
        }

        if (hadOr) {
            depItem = depends.last();
            depends.removeLast();
        }

        if (op & pkgCache::Dep::Or) {
            hadOr = true;
            // Remove the Or bit from the op so we can assign to a RelationType
            op = (op & ~pkgCache::Dep::Or);
        } else {
            hadOr = false;
        }

        DependencyInfo info(QString::fromStdString(package),
                            QString::fromStdString(version),
                            (RelationType)op, type);
        depItem.append(info);

        depends.append(depItem);
    }

    return depends;
}

QString DependencyInfo::packageName() const
{
    return m_packageName;
}

QString DependencyInfo::packageVersion() const
{
    return m_packageVersion;
}

RelationType DependencyInfo::relationType() const
{
    return m_relationType;
}

DependencyType DependencyInfo::dependencyType() const
{
    return m_dependencyType;
}

}
