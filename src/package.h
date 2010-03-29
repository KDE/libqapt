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

#ifndef PACKAGE_H
#define PACKAGE_H

#include <QtCore/QObject>

#include <apt-pkg/pkgcache.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/depcache.h>

namespace QApt {

class PackagePrivate;
class Package : public QObject
{
    Q_OBJECT
    Q_ENUMS(PackageState)
    Q_ENUMS(UpdateImportance)
public:
    Package(QObject* parent, pkgDepCache *depcache,
            pkgRecords *records, pkgCache::PkgIterator &packageIter);

    virtual ~Package();

    typedef QList<Package*> List;

    pkgDepCache *m_depCache;
    pkgRecords *m_records;
    pkgCache::PkgIterator *m_packageIter;

    QString name() const;
    QString version() const;
    QString section() const;
    QString sourcePackage() const;
    QString shortDescription() const;
    QString maintainer() const;
    QString installedVersion() const;
    QString availableVersion() const;
    QString priority() const;
    QStringList installedFilesList() const;
    QString longDescription() const;
    qint32 installedSize() const;
    qint32 availableInstalledSize() const;
    qint32 availablePackageSize() const;
    int state();

    bool isInstalled();
    QStringList requiredByList();

    void setState();

    /* "//" == TODO */
    enum PackageState {
        ToKeep = 1 << 0,
        ToInstall = 1 << 1,
        NewInstall = 1 << 2,
        ToReInstall = 1 << 3,//
        ToUpgrade = 1 << 4,
        ToDowngrade = 1 << 5,
        ToRemove = 1 << 6,
        Held = 1 << 7,
        Installed = 1 << 8,
        Upgradeable = 1 << 9,
        NowBroken = 1 << 10,
        InstallBroken = 1 << 11,
        Orphaned = 1 << 12,//
        Pinned = 1 << 13,//
        New = 1 << 14, //
        ResidualConfig = 1 << 15,
        NotInstallable = 1 << 16,
        ToPurge = 1 << 17,//
        IsImportant = 1 << 18,
        OverrideVersion = 1 << 19,//
        IsAuto = 1 << 20,
        IsGarbage = 1 << 21,
        NowPolicyBroken = 1 << 22,
        InstallPolicyBroken = 1 << 23
    };
    Q_DECLARE_FLAGS(PackageStates, PackageState)

    enum UpdateImportance {
        UnknownImportance = 1,//
        NormalImportance = 2,//
        CriticalImportance = 3,//
        SecurityImportance = 4//
    };
    Q_DECLARE_FLAGS(UpdateImportances, UpdateImportance)

private:
    PackagePrivate * const d;
};

}

#endif
