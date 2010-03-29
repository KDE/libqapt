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
        Unknown = 0,
        Keep = 1,  //
        ToInstall = 2, //
        NewInstall = 3,//
        ToReInstall = 4,//
        Upgradable = 5,//
        ToUpgrade = 6,//
        ToDowngrade = 7,//
        ToRemove = 8,//
        Held = 9,//
        Installed = 10,//
        Outdated = 11,//
        NowBroken = 12,//
        InstallBroken = 13,//
        Orphaned = 14,//
        Pinned = 15,//
        ResidualConfig = 16,//
        IsAuto = 17,//
        IsGarbage = 18,//
        NowPolicyBroken = 19,//
        InstPolicyBroken = 20//
    };

    enum UpdateImportance {
        UnknownImportance = 1,//
        NormalImportance = 2,//
        CriticalImportance = 3,//
        SecurityImportance = 4//
    };

private:
    PackagePrivate * const d;
};

}

#endif
