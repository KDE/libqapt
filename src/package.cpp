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

// Qt includes
#include <QtCore/QFile>
#include <QtCore/QStringBuilder>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTextStream>
#include <QDebug>

// Apt includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

#include <algorithm>

#include "backend.h"

namespace QApt {

class PackagePrivate
{
    public:
        QApt::Backend *backend;
        pkgDepCache *depCache;
        pkgRecords *records;
        pkgCache::PkgIterator *packageIter;
        int state;
};

Package::Package(QApt::Backend* parent, pkgDepCache *depCache,
                 pkgRecords *records, pkgCache::PkgIterator &packageIter)
        : QObject(parent)
        , d_ptr(new PackagePrivate())
{
    Q_D(Package);

    // Passing the pkgIter by pointer from Backend results in a crash
    // the first time you try to call a method needing it :(
    d->packageIter = new pkgCache::PkgIterator(packageIter);
    d->backend = parent;
    d->records= records;
    d->depCache = depCache;
    d->state = state();
}

Package::~Package()
{
    delete d_ptr;
}

QString Package::name() const
{
    Q_D(const Package);

    QString name = QString(d->packageIter->Name());

    if (!name.isEmpty()) {
        return name;
    } else {
        return QString();
    }
}

int Package::id() const
{
    Q_D(const Package);

    int id = (*d->packageIter)->ID;

    return id;
}

QString Package::section() const
{
    Q_D(const Package);

    QString section = QString(d->packageIter->Section());

    if (!section.isEmpty()) {
        return section;
    } else {
        return QString();
    }
}

QString Package::sourcePackage() const
{
    Q_D(const Package);

    QString sourcePackage;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    pkgRecords::Parser &rec = d->records->Lookup(ver.FileList());
    sourcePackage = QString::fromStdString(rec.SourcePkg());
    // If empty, use name. (Name would equal source package in that case)
    if (sourcePackage.isEmpty()) {
        sourcePackage = name();
    }

    return sourcePackage;
}

QString Package::shortDescription() const
{
    Q_D(const Package);

    QString shortDescription;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->records->Lookup(Desc.FileList());
        shortDescription = QString::fromStdString(parser.ShortDesc());
        return shortDescription;
    }

    return shortDescription;
}

QString Package::longDescription() const
{
    Q_D(const Package);

    QString rawDescription;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    if (!ver.end()) {
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->records->Lookup(Desc.FileList());
        rawDescription = QString::fromStdString(parser.LongDesc());
        // Apt acutally returns the whole description, we just want the
        // extended part.
        rawDescription.remove(shortDescription() + "\n");
        qDebug() << rawDescription;
        // Now we're really "raw". Sort of. ;)

        QString parsedDescription;
        // Split at double newline, by "section"
        QStringList sections = rawDescription.split("\n .");

        int i;
        for (i = 0; i < sections.count(); ++i) {
            sections[i].replace(QRegExp("\n( |\t)+(-|\\*)"), "\n\r " + QString::fromUtf8("\xE2\x80\xA2"));
            // There should be no new lines within a section.
            sections[i].remove('\n');
            // Hack to get the lists working again.
            sections[i].replace('\r', '\n');
            // Merge multiple whitespace chars into one
            sections[i].replace(QRegExp("\\ \\ +"), QString(' '));
            // Remove the initial whitespace
            sections[i].remove(0, 1);
            // Append to parsedDescription
            if (sections[i].startsWith("\n " + QString::fromUtf8("\xE2\x80\xA2 ")) || i == 0) {
                parsedDescription += sections[i];
            }  else {
                parsedDescription += "\n\n" + sections[i];
            }
        }

        return parsedDescription;
    } else {
        return QString();
    }
}

QString Package::maintainer() const
{
    Q_D(const Package);

    QString maintainer;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser & parser = d->records->Lookup(ver.FileList());
        maintainer = QString::fromStdString(parser.Maintainer());
        // FIXME: QLabel interprets < and > as html tags and cuts off the email address
        return maintainer;
    }
    return maintainer;
}

QString Package::homepage() const
{
    Q_D(const Package);

    QString homepage;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
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

    if ((*d->packageIter)->CurrentVer == 0) {
        pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
        if (d->state & NotInstallable) {
            return QString();
        } else {
            return QString::fromStdString(State.CandidateVerIter(*d->depCache).VerStr());
        }
    } else {
        return QString::fromStdString(d->packageIter->CurrentVer().VerStr());
    }
}

QString Package::installedVersion() const
{
    Q_D(const Package);

    QString installedVersion;
    if ((*d->packageIter)->CurrentVer == 0) {
        return QString();
    }
    installedVersion = QString::fromStdString(d->packageIter->CurrentVer().VerStr());
    return installedVersion;
}

QString Package::availableVersion() const
{
    Q_D(const Package);

    QString availableVersion;
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
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
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
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

QString Package::origin() const
{
    Q_D(const Package);

    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

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
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
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

QUrl Package::changelogUrl() const
{
    QString prefix;
    QString srcPackage = sourcePackage();
    QString sourceSection = section();

    if (sourceSection.contains('/')) {
        QStringList split = sourceSection.split('/');
        sourceSection = split[0];
    } else {
        sourceSection = "main";
    }

    prefix = srcPackage[0];
    if (srcPackage.size() > 3 && srcPackage.startsWith(QLatin1String("lib"))) {
        prefix = "lib" % srcPackage[3];
    }

    QString versionString;
    if (!availableVersion().isNull()) {
        versionString = availableVersion();
    }

    if (versionString.contains(':')) {
        QStringList epochVersion = versionString.split(':');
        // If the version has an epoch, take the stuff after the epoch
        versionString = epochVersion[1];
    }

    QString urlBase = "http://changelogs.ubuntu.com/changelogs/pool/";
    QUrl url = QUrl(urlBase % sourceSection % '/' % prefix % '/' %
                    srcPackage % '/' % srcPackage % '_' % versionString % '/'
                    % "changelog");

    qDebug() << url;

    return url;
}

QUrl Package::screenshotUrl(QApt::ScreenshotType type) const
{
    QString urlBase;
    switch (type) {
        case QApt::Thumbnail:
            urlBase = "http://screenshots.debian.net/thumbnail/";
            break;
        case QApt::Screenshot:
            urlBase = "http://screenshots.debian.net/screenshot/";
            break;
    }

    QUrl url = QUrl(urlBase % name());

    return url;
}

QString Package::supportedUntil()
{
    if (!isSupported()) {
        return QString();
    }

    pkgTagSection sec;
    time_t releaseDate = -1;
    QString release;

    QFile lsb_release("/etc/lsb-release");
    if (!lsb_release.open(QFile::ReadOnly)) {
        // Though really, your system is screwed if this happens...
        return QString();
    }

    QTextStream stream(&lsb_release);
    QString line;
    do {
        line = stream.readLine();
        QStringList split = line.split('=');
        if (split[0] == "DISTRIB_CODENAME") {
            release = split[1];
        }
    } while (!line.isNull());

    // Canonical only provides support for Ubuntu, but we don't have to worry
    // about Debian systems as long as we assume that this function can fail.
    QString releaseFile = getReleaseFileForOrigin("Ubuntu", release);

    if(!FileExists(releaseFile.toStdString())) {
        // happens e.g. when there is no release file and is harmless
        return QString();
    }

    // read the relase file
    FileFd fd(releaseFile.toStdString(), FileFd::ReadOnly);
    pkgTagFile tag(&fd);
    tag.Step(sec);

    if(!StrToTime(sec.FindS("Date"), releaseDate)) {
        return QString();
    }

    //FIXME: Figure out how to get the value from the package record
    const int supportTime = 18; // months

    QDateTime supportEnd = QDateTime::fromTime_t(releaseDate).addMonths(supportTime);

    return supportEnd.toString("MMMM yyyy");
}

qint32 Package::installedSize() const
{
    Q_D(const Package);

    pkgCache::VerIterator ver = d->packageIter->CurrentVer();

    // If we are installed
    if (d->state & Installed) {
        return ver->InstalledSize;
    // Else we aren't installed
    } else {
        pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
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

    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    if (d->state & NotInstallable) {
        return -1;
    }

    return State.CandidateVerIter(*d->depCache)->Size;
}

int Package::state()
{
    Q_D(const Package);

    int packageState = 0;

    pkgDepCache::StateCache &stateCache = (*d->depCache)[*d->packageIter];
    pkgCache::VerIterator ver = d->packageIter->CurrentVer();

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

    if ((*d->packageIter)->Flags & (pkgCache::Flag::Important |
                              pkgCache::Flag::Essential)) {
        packageState |= IsImportant;
    }

    if ((*d->packageIter)->CurrentState == pkgCache::State::ConfigFiles) {
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

    return (d->state & Installed);
}

bool Package::isValid()
{
    Q_D(Package);

    // As long as it's not NotInstallable, it'll be valid
    return !(d->state & NotInstallable);
}

bool Package::isSupported()
{
    bool supported = (origin() == "Ubuntu" && (component() == "main" || component() == "restricted")
                      && isTrusted());

    return supported;
}

QStringList Package::dependencyList(bool useCanidateVersion) const
{
    Q_D(const Package);

    // TODO: Stub, won't return anything.
    QStringList dependsList;
    pkgCache::VerIterator current;

    if(!useCanidateVersion) {
        current = (*d->depCache)[*d->packageIter].InstVerIter(*d->depCache);
    }
    if(useCanidateVersion || current.end()) {
        current = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    }

    // no information found
    if(current.end()) {
        return dependsList;
    }

    return dependsList;
}

QStringList Package::requiredByList() const
{
    Q_D(const Package);

    QStringList reverseDependsList;

    for(pkgCache::DepIterator it = d->packageIter->RevDependsList(); it.end() != true; ++it) {
        reverseDependsList << QString::fromStdString(it.ParentPkg().Name());
    }

    return reverseDependsList;
}

QStringList Package::providesList() const
{
    Q_D(const Package);

    QStringList provides;

    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    if (d->state & NotInstallable) {
        return provides;
    }

    for (pkgCache::PrvIterator Prv =
         State.CandidateVerIter(*d->depCache).ProvidesList(); Prv.end() != true;
         ++Prv) {
        provides.push_back(Prv.Name());
    }

   return provides;
}

bool Package::isTrusted()
{
    Q_D(Package);

    pkgCache::VerIterator Ver;
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    Ver = State.CandidateVerIter(*d->depCache);
    if (Ver == 0) {
        return false;
    }

    pkgSourceList *Sources = d->backend->packageSourceList();
    for (pkgCache::VerFileIterator i = Ver.FileList(); i.end() == false; ++i)
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

    d->depCache->MarkAuto(*d->packageIter, flag);
}


void Package::setKeep()
{
    Q_D(Package);

    d->depCache->MarkKeep(*d->packageIter, false);
    d->depCache->SetReInstall(*d->packageIter, false);
    d->backend->packageChanged(this);
}

void Package::setInstall()
{
    Q_D(Package);

    d->depCache->MarkInstall(*d->packageIter, true);

    // FIXME: can't we get rid of it here?
    // if there is something wrong, try to fix it
    if (!d->state & ToInstall || d->depCache->BrokenCount() > 0) {
        pkgProblemResolver Fix(d->depCache);
        Fix.Clear(*d->packageIter);
        Fix.Protect(*d->packageIter);
        Fix.Resolve(true);
    }

    d->backend->packageChanged(this);
}

void Package::setReInstall()
{
    Q_D(Package);

    d->depCache->SetReInstall(*d->packageIter, true);
    d->backend->packageChanged(this);
}


void Package::setRemove()
{
    Q_D(Package);

    pkgProblemResolver Fix(d->depCache);

    Fix.Clear(*d->packageIter);
    Fix.Protect(*d->packageIter);
    Fix.Remove(*d->packageIter);

    Fix.InstallProtect();
    Fix.Resolve(true);

    d->depCache->SetReInstall(*d->packageIter, false);
    d->depCache->MarkDelete(*d->packageIter, false);

    d->backend->packageChanged(this);
}

void Package::setPurge()
{
    Q_D(Package);

    pkgProblemResolver Fix(d->depCache);

    Fix.Clear(*d->packageIter);
    Fix.Protect(*d->packageIter);
    Fix.Remove(*d->packageIter);

    Fix.InstallProtect();
    Fix.Resolve(true);

    d->depCache->SetReInstall(*d->packageIter, false);
    d->depCache->MarkDelete(*d->packageIter, true);

    d->backend->packageChanged(this);
}

pkgCache::PkgFileIterator Package::searchPkgFileIter(const QString &label, const QString &release)
{
    Q_D(Package);

    pkgCache::VerIterator verIter;
    pkgCache::VerFileIterator verFileIter;
    pkgCache::PkgFileIterator found;

    for(verIter = d->packageIter->VersionList(); !verIter.end(); ++verIter) {
        for(verFileIter = verIter.FileList(); !verFileIter.end(); ++verFileIter) {
            for(found = verFileIter.File(); !found.end(); ++found) {
                if(!found.end() && found.Label() &&
                  QString::fromStdString(found.Label()) == label &&
                  found.Origin() && QString::fromStdString(found.Origin()) == label &&
                  found.Archive() && QString::fromStdString(found.Archive()) == release) {
                    //cerr << "found: " << PF.FileName() << endl;
                    return found;
                }
            }
        }
    }
   found = pkgCache::PkgFileIterator(*d->packageIter->Cache());
   return found;
}

QString Package::getReleaseFileForOrigin(const QString &label, const QString &release)
{
    Q_D(Package);

    pkgIndexFile *index;
    pkgCache::PkgFileIterator found = searchPkgFileIter(label, release);

    if (found.end()) {
        return QString();
    }

    // search for the matching meta-index
    pkgSourceList *list = d->backend->packageSourceList();

    if(list->FindIndex(found, index)) {
        vector<metaIndex *>::const_iterator I;
        for(I=list->begin(); I != list->end(); ++I) {
            vector<pkgIndexFile *>  *ifv = (*I)->GetIndexFiles();
            if(find(ifv->begin(), ifv->end(), index) != ifv->end()) {
                string stduri = _config->FindDir("Dir::State::lists");
                stduri += URItoFileName((*I)->GetURI());
                stduri += "dists_";
                stduri += (*I)->GetDist();
                stduri += "_Release";

                QString uri = QString::fromStdString(stduri);
                return uri;
            }
        }
    }

    return QString();
}

}
