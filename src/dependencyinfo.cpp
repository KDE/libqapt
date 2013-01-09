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

class DependencyInfoPrivate : public QSharedData {
public:
    DependencyInfoPrivate()
        : QSharedData()
        , relationType(NoOperand)
        , dependencyType(InvalidType) {}

    DependencyInfoPrivate(const QString &package, const QString &version,
                          RelationType rType, DependencyType dType)
        : QSharedData()
        , packageName(package)
        , packageVersion(version)
        , relationType(rType)
        , dependencyType(dType) {}

    DependencyInfoPrivate(const DependencyInfoPrivate &other)
        : QSharedData(other)
        , packageName(other.packageName)
        , packageVersion(other.packageVersion)
        , relationType(other.relationType)
        , dependencyType(other.dependencyType) {}

    QString packageName;
    QString packageVersion;
    RelationType relationType;
    DependencyType dependencyType;
};

DependencyInfo::DependencyInfo()
    : d(new DependencyInfoPrivate())
{
}

DependencyInfo::DependencyInfo(const QString &package, const QString &version,
                               RelationType rType, DependencyType dType)
    : d(new DependencyInfoPrivate(package, version, rType, dType))
{
}

DependencyInfo::DependencyInfo(const DependencyInfo &other)
{
    d = other.d;
}

DependencyInfo::~DependencyInfo()
{
}

DependencyInfo &DependencyInfo::operator=(const DependencyInfo &rhs)
{
    // Protect against self-assignment
    if (this == &rhs) {
        return *this;
    }
    d = rhs.d;
    return *this;
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
    return d->packageName;
}

QString DependencyInfo::packageVersion() const
{
    return d->packageVersion;
}

RelationType DependencyInfo::relationType() const
{
    return d->relationType;
}

DependencyType DependencyInfo::dependencyType() const
{
    return d->dependencyType;
}

QString DependencyInfo::typeName(DependencyType type)
{
    return QLatin1String(pkgCache::DepType(type));
}

}
