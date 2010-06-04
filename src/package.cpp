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

#include "package.h"

#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

#include <apt-pkg/algorithms.h>

#include "backend.h"

namespace QApt {

class PackagePrivate
{
    public:
        QApt::Backend *backend;
        pkgRecords *records;
        pkgDepCache *depCache;
        int state;
};

Package::Package(QApt::Backend* parent, pkgDepCache *depCache,
                 pkgRecords *records, pkgCache::PkgIterator &packageIter)
        : QObject(parent)
        , d_ptr(new PackagePrivate())
{
    Q_D(Package);
    m_packageIter = new pkgCache::PkgIterator(packageIter);
    d->backend = parent;
    d->records= records;
    d->depCache = depCache;
    d->state = state();
}

Package::~Package()
{
    delete d_ptr;
    delete m_packageIter;
}

QString Package::name() const
{
    QString name = QString(m_packageIter->Name());

    if (!name.isNull()) {
        return name;
    } else {
        return QString();
    }
}

int Package::id() const
{
    int id = (*m_packageIter)->ID;

    return id;
}

QString Package::section() const
{
    QString section = QString(m_packageIter->Section());

    if (!section.isNull()) {
        return section;
    } else {
        return QString();
    }
}

QString Package::sourcePackage() const
{
    Q_D(const Package);

    QString sourcePackage;
    pkgCache::VerIterator ver = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);
    pkgRecords::Parser &rec=d->records->Lookup(ver.FileList());
    sourcePackage = QString::fromStdString(rec.SourcePkg());

    return sourcePackage;
}

QString Package::shortDescription() const
{
    Q_D(const Package);

    QString shortDescription;
    pkgCache::VerIterator ver = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->records->Lookup(Desc.FileList());
        shortDescription = QString::fromStdString(parser.ShortDesc());
        return shortDescription;
    }

    return shortDescription;
}

QString Package::maintainer() const
{
    Q_D(const Package);

    QString maintainer;
    pkgCache::VerIterator ver = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser & parser = d->records->Lookup(ver.FileList());
        maintainer = QString::fromStdString(parser.Maintainer());
        return maintainer;
    }
    return maintainer;
}

QString Package::homepage() const
{
    Q_D(const Package);

    QString homepage;
    pkgCache::VerIterator ver = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser & parser = d->records->Lookup(ver.FileList());
        homepage = QString::fromStdString(parser.Homepage());
        return homepage;
    }
    return homepage;
}

QString Package::version() const
{
    Q_D(const Package);

    if ((*m_packageIter)->CurrentVer == 0) {
        pkgDepCache::StateCache & State = (*d->depCache)[*m_packageIter];
        if (d->state & NotInstallable) {
            return QString();
        } else {
            return QString::fromStdString(State.CandidateVerIter(*d->depCache).VerStr());
        }
    } else {
        return QString::fromStdString(m_packageIter->CurrentVer().VerStr());
    }
}

QString Package::installedVersion() const
{
    QString installedVersion;
    if ((*m_packageIter)->CurrentVer == 0) {
        return QString();
    }
    installedVersion = QString::fromStdString(m_packageIter->CurrentVer().VerStr());
    return installedVersion;
}

QString Package::availableVersion() const
{
    Q_D(const Package);

    QString availableVersion;
    pkgDepCache::StateCache & State = (*d->depCache)[*m_packageIter];
    if (d->state & NotInstallable) {
        return QString();
    }

    availableVersion = QString::fromStdString(State.CandidateVerIter(*d->depCache).VerStr());
    return availableVersion;
}

QString Package::priority() const
{
    Q_D(const Package);

    QString priority;
    pkgCache::VerIterator ver = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);
    if (ver != 0) {
        priority = QString::fromStdString(ver.PriorityType());
        return priority;
    } else {
        return QString();
    }
}

QStringList Package::installedFilesList() const
{
    QStringList installedFilesList;
    QFile infoFile("/var/lib/dpkg/info/" + name() + ".list");

    if (infoFile.open(QFile::ReadOnly)) {
        QTextStream stream(&infoFile);
        QString line;

        do {
            line = stream.readLine();
            installedFilesList << line;
        } while (!line.isNull());

        // The first item won't be a file
        installedFilesList.removeFirst();
    }

    return installedFilesList;
}

QString Package::longDescription() const
{
    Q_D(const Package);

    QString longDescription;
    pkgCache::VerIterator ver = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);

    if (!ver.end()) {
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->records->Lookup(Desc.FileList());
        longDescription = QString::fromStdString(parser.LongDesc());
        // Dpkg uses a line with a space and a dot to mark a double newline.
        // It's not really human-readable, though, so remove it.
        longDescription.replace(QLatin1String("\n .\n"), QLatin1String("\n\n"));
        return longDescription;
    } else {
        return QString();
    }
}

QString Package::origin() const
{
    Q_D(const Package);

    pkgCache::VerIterator Ver = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);

    if(!Ver.end()) {
         pkgCache::VerFileIterator VF = Ver.FileList();
         return VF.File().Origin();
    }

   return QString();
}

QString Package::component() const
{
    Q_D(const Package);

    QString res;
    pkgCache::VerIterator Ver;
    pkgDepCache::StateCache & State = (*d->depCache)[*m_packageIter];
    if (d->state & NotInstallable) {
        return QString();
    }
    Ver = State.CandidateVerIter(*d->depCache);
    pkgCache::VerFileIterator VF = Ver.FileList();
    pkgCache::PkgFileIterator File = VF.File();

    if(File.Component() == NULL) {
        return QString();
    }

    res = QString::fromStdString(File.Component());

    return res;
}

qint32 Package::installedSize() const
{
    Q_D(const Package);

    pkgCache::VerIterator ver = m_packageIter->CurrentVer();

    // If we are installed
    if (d->state & Installed) {
        return ver->InstalledSize;
    // Else we aren't installed
    } else {
        pkgDepCache::StateCache & State = (*d->depCache)[*m_packageIter];
        if (d->state & NotInstallable) {
            // Nonexistent package
            return -1;
        }
        return State.CandidateVerIter(*d->depCache)->InstalledSize;
    }
}

qint32 Package::downloadSize() const
{
    Q_D(const Package);

    pkgDepCache::StateCache & State = (*d->depCache)[*m_packageIter];
    if (d->state & NotInstallable) {
        return -1;
    }

    return State.CandidateVerIter(*d->depCache)->Size;
}

int Package::state()
{
    Q_D(const Package);

    int packageState = 0;

    pkgDepCache::StateCache &stateCache = (*d->depCache)[*m_packageIter];
    pkgCache::VerIterator ver = m_packageIter->CurrentVer();

    if (stateCache.Install()) {
        packageState |= ToInstall;
    }

    if (stateCache.iFlags & pkgDepCache::ReInstall) {
        packageState |= ToReInstall;
    } else if (stateCache.NewInstall()) { // Order matters here.
        packageState |= NewInstall;
    } else if (stateCache.Upgrade()) {
        packageState |= ToUpgrade;
    } else if (stateCache.Downgrade()) {
        packageState |= ToDowngrade;
    } else if (stateCache.Delete()) {
        packageState |= ToRemove;
        if (stateCache.iFlags & pkgDepCache::Purge) {
            packageState |= ToPurge;
        }
    } else if (stateCache.Keep()) {
        packageState |= ToKeep;
    }

    if (!ver.end()) {
        packageState |= Installed;

        if (stateCache.Upgradable() && stateCache.CandidateVer != NULL) {
            packageState |= Upgradeable;
            if (stateCache.Keep()) {
                packageState |= Held;
            }
      }

        if (stateCache.Downgrade()) {
            packageState |= ToDowngrade;
        }
    }

    if (stateCache.NowBroken()) {
        packageState |= NowBroken;
    }

    if (stateCache.InstBroken()) {
        packageState |= InstallBroken;
    }

    if ((*m_packageIter)->Flags & (pkgCache::Flag::Important |
                              pkgCache::Flag::Essential)) {
        packageState |= IsImportant;
    }

    if ((*m_packageIter)->CurrentState == pkgCache::State::ConfigFiles) {
        packageState |= ResidualConfig;
    }

    if (stateCache.CandidateVer == 0 ||
        !stateCache.CandidateVerIter(*d->depCache).Downloadable()) {
        packageState |= NotInstallable;
    }

    if (stateCache.Flags & pkgCache::Flag::Auto) {
        packageState |= IsAuto;
    }

    if (stateCache.Garbage) {
        packageState |= IsGarbage;
    }

    if (stateCache.NowPolicyBroken()) {
        packageState |= NowPolicyBroken;
    }

    if (stateCache.InstPolicyBroken()) {
        packageState |= InstallPolicyBroken;
    }

   return packageState /*| _boolFlags*/;
}

bool Package::isInstalled()
{
    Q_D(Package);

    if (d->state & Installed) {
        return true;
    } else {
        return false;
    }
}

bool Package::isValid()
{
    Q_D(Package);

    if (d->state & NotInstallable) {
        return false;
    } else {
        return true;
    }
}

QStringList Package::dependencyList(bool useCanidateVersion) const
{
    Q_D(const Package);

    // TODO: Stub, won't return anything.
    QStringList dependsList;
    pkgCache::VerIterator current;

    if(!useCanidateVersion) {
        current = (*d->depCache)[*m_packageIter].InstVerIter(*d->depCache);
    }
    if(useCanidateVersion || current.end()) {
        current = (*d->depCache)[*m_packageIter].CandidateVerIter(*d->depCache);
    }

    // no information found
    if(current.end()) {
        return dependsList;
    }

    return dependsList;
}

QStringList Package::requiredByList() const
{
    QStringList reverseDependsList;

    for(pkgCache::DepIterator it = m_packageIter->RevDependsList(); it.end() != true; it++) {
        reverseDependsList << QString::fromStdString(it.ParentPkg().Name());
    }

    return reverseDependsList;
}

QStringList Package::providesList() const
{
    Q_D(const Package);

    QStringList provides;

    pkgDepCache::StateCache & State = (*d->depCache)[*m_packageIter];
    if (d->state & NotInstallable) {
        return provides;
    }

    for (pkgCache::PrvIterator Prv =
         State.CandidateVerIter(*d->depCache).ProvidesList(); Prv.end() != true;
         Prv++) {
        provides.push_back(Prv.Name());
    }

   return provides;
}

bool Package::isTrusted()
{
    Q_D(Package);

    pkgCache::VerIterator Ver;
    pkgDepCache::StateCache & State = (*d->depCache)[*m_packageIter];
    Ver = State.CandidateVerIter(*d->depCache);
    if (Ver == 0) {
        return false;
    }

    pkgSourceList *Sources = d->backend->packageSourceList();
    for (pkgCache::VerFileIterator i = Ver.FileList(); i.end() == false; i++)
    {
        pkgIndexFile *Index;
        if (Sources->FindIndex(i.File(),Index) == false) {
           continue;
        }
        if (Index->IsTrusted()) {
            return true;
        }
    }

    return false;
}

bool Package::wouldBreak()
{
    Q_D(Package);

    if ((d->state & ToRemove) || (!(d->state & Installed) && (d->state & ToKeep))) {
        return false;
    }
    return d->state & InstallBroken;
}

void Package::setAuto(bool flag)
{
    Q_D(Package);

    d->depCache->MarkAuto(*m_packageIter, flag);
}


void Package::setKeep()
{
    Q_D(Package);

    d->depCache->MarkKeep(*m_packageIter, false);
    d->backend->packageChanged(this);
    setReInstall(false);
}

void Package::setInstall()
{
    Q_D(Package);

    d->depCache->MarkInstall(*m_packageIter, true);

    // FIXME: can't we get rid of it here?
    // if there is something wrong, try to fix it
    if (!d->state & ToInstall || d->depCache->BrokenCount() > 0) {
        pkgProblemResolver Fix(d->depCache);
        Fix.Clear(*m_packageIter);
        Fix.Protect(*m_packageIter);
        Fix.Resolve(true);
    }

    d->backend->packageChanged(this);
}

void Package::setReInstall(bool flag)
{
    Q_D(Package);

    d->depCache->SetReInstall(*m_packageIter, flag);
    d->backend->packageChanged(this);
}


void Package::setRemove(bool purge)
{
    Q_D(Package);

    pkgProblemResolver Fix(d->depCache);

    Fix.Clear(*m_packageIter);
    Fix.Protect(*m_packageIter);
    Fix.Remove(*m_packageIter);

    Fix.InstallProtect();
    Fix.Resolve(true);

    d->depCache->SetReInstall(*m_packageIter, false);
    d->depCache->MarkDelete(*m_packageIter, purge);

    d->backend->packageChanged(this);
}

}
