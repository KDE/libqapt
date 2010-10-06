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
#include <apt-pkg/versionmatch.h>

#include <algorithm>

#include "backend.h"
#include "cache.h"

namespace QApt {

class PackagePrivate
{
    public:
        QApt::Backend *backend;
        pkgDepCache *depCache;
        pkgRecords *records;
        pkgCache::PkgIterator *packageIter;
        int state;

        pkgCache::PkgFileIterator searchPkgFileIter(const QString &label, const QString &release) const;
        QString getReleaseFileForOrigin(const QString &label, const QString &release) const;
};

pkgCache::PkgFileIterator PackagePrivate::searchPkgFileIter(const QString &label, const QString &release) const
{
    pkgCache::VerIterator verIter = packageIter->VersionList();
    pkgCache::VerFileIterator verFileIter;
    pkgCache::PkgFileIterator found;

    while (!verIter.end()) {
        for (verFileIter = verIter.FileList(); !verFileIter.end(); ++verFileIter) {
            for(found = verFileIter.File(); !found.end(); ++found) {
                const char *verLabel = found.Label();
                const char *verOrigin = found.Origin();
                const char *verArchive = found.Archive();
                if (verLabel && verOrigin && verArchive) {
                    if(QString::fromStdString(verLabel) == label &&
                      QString::fromStdString(verOrigin) == label &&
                      QString::fromStdString(verArchive) == release) {
                          return found;
                    }
                }
            }
        }
        ++verIter;
    }
   found = pkgCache::PkgFileIterator(*packageIter->Cache());
   return found;
}

QString PackagePrivate::getReleaseFileForOrigin(const QString &label, const QString &release) const
{
    pkgCache::PkgFileIterator found = searchPkgFileIter(label, release);

    if (found.end()) {
        return QString();
    }

    // search for the matching meta-index
    pkgSourceList *list = backend->packageSourceList();
    pkgIndexFile *index;

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

Package::Package(QApt::Backend* backend, pkgDepCache *depCache,
                 pkgRecords *records, pkgCache::PkgIterator &packageIter)
        : d(new PackagePrivate())
{
    // Passing the pkgIter by pointer from Backend results in a crash
    // the first time you try to call a method needing it :(
    // Probably because the pointer is created inside an iterator and
    // is very temporary.
    d->packageIter = new pkgCache::PkgIterator(packageIter);
    d->backend = backend;
    d->records= records;
    d->depCache = depCache;
    d->state = 0;
}

Package::~Package()
{
    delete d;
}

pkgCache::PkgIterator *Package::packageIterator() const
{
    return d->packageIter;
}

QString Package::name() const
{
    QString name(d->packageIter->Name());

    return name;
}

QLatin1String Package::latin1Name() const
{
    QLatin1String name(d->packageIter->Name());

    return name;
}

int Package::id() const
{
    int id = (*d->packageIter)->ID;

    return id;
}

QString Package::section() const
{
    const char *s = d->packageIter->Section();

    if (s) {
        return QString(s);
    }

    return QString();
}

QString Package::sourcePackage() const
{
    QString sourcePackage;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser &rec = d->records->Lookup(ver.FileList());
        sourcePackage = QString::fromStdString(rec.SourcePkg());
    }

    // If empty, use name. (Name would equal source package in that case)
    if (sourcePackage.isEmpty()) {
        sourcePackage = name();
    }

    return sourcePackage;
}

QString Package::shortDescription() const
{
    QString shortDescription;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->records->Lookup(Desc.FileList());
        shortDescription = QString::fromUtf8(parser.ShortDesc().data());
        return shortDescription;
    }

    return shortDescription;
}

QString Package::longDescription() const
{
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    if (!ver.end()) {
        QString rawDescription;
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->records->Lookup(Desc.FileList());
        rawDescription = QString::fromUtf8(parser.LongDesc().data());
        // Apt acutally returns the whole description, we just want the
        // extended part.
        rawDescription.remove(shortDescription() % '\n');
        // *Now* we're really raw. Sort of. ;)

        QString parsedDescription;
        // Split at double newline, by "section"
        QStringList sections = rawDescription.split("\n .");

        int i;
        for (i = 0; i < sections.count(); ++i) {
            sections[i].replace(QRegExp("\n( |\t)+(-|\\*)"), "\n\r " % QString::fromUtf8("\xE2\x80\xA2"));
            // There should be no new lines within a section.
            sections[i].remove(QLatin1Char('\n'));
            // Hack to get the lists working again.
            sections[i].replace(QLatin1Char('\r'), QLatin1Char('\n'));
            // Merge multiple whitespace chars into one
            sections[i].replace(QRegExp("\\ \\ +"), QChar(' '));
            // Remove the initial whitespace
            sections[i].remove(0, 1);
            // Append to parsedDescription
            if (sections[i].startsWith(QLatin1String("\n ") % QString::fromUtf8("\xE2\x80\xA2 ")) || i == 0) {
                parsedDescription += sections[i];
            }  else {
                parsedDescription += "\n\n" % sections[i];
            }
        }
        return parsedDescription;
    }

    return QString();
}

QString Package::maintainer() const
{
    QString maintainer;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser & parser = d->records->Lookup(ver.FileList());
        maintainer = QString::fromUtf8(parser.Maintainer().data());
        maintainer.replace(QChar('<'), QLatin1String("&lt;"));
    }
    return maintainer;
}

QString Package::homepage() const
{
    QString homepage;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser & parser = d->records->Lookup(ver.FileList());
        homepage = QString::fromStdString(parser.Homepage());
    }
    return homepage;
}

QString Package::version() const
{
    if ((*d->packageIter)->CurrentVer == 0) {
        pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
        if (State.CandidateVer == 0) {
            return QString();
        } else {
            return QString::fromStdString(State.CandidateVerIter(*d->depCache).VerStr());
        }
    } else {
        return QString::fromStdString(d->packageIter->CurrentVer().VerStr());
    }
}

QStringList Package::availableVersions() const
{
    QStringList versions;

    // Get available Versions.
    for (pkgCache::VerIterator Ver = d->packageIter->VersionList();
         Ver.end() == false; ++Ver) {

        // We always take the first available version.
        pkgCache::VerFileIterator VF = Ver.FileList();
        if (!VF.end()) {
            pkgCache::PkgFileIterator File = VF.File();

            if (File->Archive != 0) {
                versions.append(QString(Ver.VerStr()) % QLatin1String(" (") %
                QString(File.Archive()) % QLatin1Char(')'));
            } else {
                versions.append(QString(Ver.VerStr()) % QLatin1String(" (") %
                QString(File.Site()) % QLatin1Char(')'));
            }
        }
    }

    return versions;
}

QString Package::installedVersion() const
{
    if ((*d->packageIter)->CurrentVer == 0) {
        return QString();
    }
    QString installedVersion = QString::fromStdString(d->packageIter->CurrentVer().VerStr());
    return installedVersion;
}

QString Package::availableVersion() const
{
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    if (State.CandidateVer == 0) {
        return QString();
    }

    QString availableVersion = QString::fromStdString(State.CandidateVerIter(*d->depCache).VerStr());
    return availableVersion;
}

QString Package::priority() const
{
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (ver != 0) {
        QString priority = QString::fromStdString(ver.PriorityType());
        return priority;
    } else {
        return QString();
    }
}

QStringList Package::installedFilesList() const
{
    QStringList installedFilesList;
    QFile infoFile("/var/lib/dpkg/info/" % name() % ".list");

    if (infoFile.open(QFile::ReadOnly)) {
        QTextStream stream(&infoFile);
        QString line;

        do {
            line = stream.readLine();
            installedFilesList << line;
        } while (!line.isNull());

        // The first item won't be a file
        installedFilesList.removeFirst();

        for (int i = 0; i < installedFilesList.size() - 1; ++i) {
            if (installedFilesList.at(i+1).contains(installedFilesList.at(i))) {
                installedFilesList[i] = ' ';
            }
        }

        installedFilesList.removeAll(QChar(' '));
    }

    return installedFilesList;
}

QString Package::origin() const
{
    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    if(!Ver.end()) {
         pkgCache::VerFileIterator VF = Ver.FileList();
         return VF.File().Origin();
    }

    return QString();
}

QString Package::component() const
{
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    if (State.CandidateVer == 0) {
        return QString();
    }
    pkgCache::VerIterator Ver = State.CandidateVerIter(*d->depCache);
    pkgCache::VerFileIterator VF = Ver.FileList();
    pkgCache::PkgFileIterator File = VF.File();

    if(File.Component() == NULL) {
        return QString();
    }

    return QString::fromStdString(File.Component());
}

QUrl Package::changelogUrl() const
{
    QString prefix;
    const QString srcPackage = sourcePackage();
    QString sourceSection = section();

    if (sourceSection.contains('/')) {
        QStringList split = sourceSection.split('/');
        sourceSection = split.at(0);
    } else {
        sourceSection = QString("main");
    }

    if (srcPackage.size() > 3 && srcPackage.startsWith(QLatin1String("lib"))) {
        prefix = QLatin1String("lib") % srcPackage[3];
    } else {
        prefix = srcPackage[0];
    }

    QString versionString;
    if (!availableVersion().isEmpty()) {
        versionString = availableVersion();
    }

    if (versionString.contains(':')) {
        QStringList epochVersion = versionString.split(':');
        // If the version has an epoch, take the stuff after the epoch
        versionString = epochVersion[1];
    }

    QString urlBase = QLatin1String("http://changelogs.ubuntu.com/changelogs/pool/");
    QUrl url = QUrl(urlBase % sourceSection % '/' % prefix % '/' %
                    srcPackage % '/' % srcPackage % '_' % versionString % '/'
                    % QLatin1String("changelog"));

    return url;
}

QUrl Package::screenshotUrl(QApt::ScreenshotType type) const
{
    QString urlBase;
    switch (type) {
        case QApt::Thumbnail:
            urlBase = QLatin1String("http://screenshots.debian.net/thumbnail/");
            break;
        case QApt::Screenshot:
            urlBase = QLatin1String("http://screenshots.debian.net/screenshot/");
            break;
    }

    QUrl url = QUrl(urlBase % name());

    return url;
}

QString Package::supportedUntil() const
{
    if (!isSupported()) {
        return QString();
    }

    QFile lsb_release("/etc/lsb-release");
    if (!lsb_release.open(QFile::ReadOnly)) {
        // Though really, your system is screwed if this happens...
        return QString();
    }

    pkgTagSection sec;
    time_t releaseDate = -1;
    QString release;

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
    QString releaseFile = d->getReleaseFileForOrigin("Ubuntu", release);

    if(!FileExists(releaseFile.toStdString())) {
        // happens e.g. when there is no release file and is harmless
        return QString();
    }

    // read the relase file
    FileFd fd(releaseFile.toStdString(), FileFd::ReadOnly);
    pkgTagFile tag(&fd);
    tag.Step(sec);

    if(!RFC1123StrToTime(sec.FindS("Date").data(), releaseDate)) {
        return QString();
    }

    //FIXME: Figure out how to get the value from the package record
    const int supportTime = 18; // months

    QDateTime supportEnd = QDateTime::fromTime_t(releaseDate).addMonths(supportTime);

    return supportEnd.toString("MMMM yyyy");
}

qint64 Package::currentInstalledSize() const
{
    pkgCache::VerIterator ver = d->packageIter->CurrentVer();

    if (!ver.end()) {
        return qint64(ver->InstalledSize);
    } else {
        return qint64(-1);
    }
}

qint64 Package::availableInstalledSize() const
{
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    if (State.CandidateVer == 0) {
        return qint64(-1);
    }
    return qint64(State.CandidateVerIter(*d->depCache)->InstalledSize);
}

qint64 Package::downloadSize() const
{
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    if (State.CandidateVer == 0) {
        return qint64(-1);
    }

    return qint64(State.CandidateVerIter(*d->depCache)->Size);
}

int Package::state() const
{
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

        if (stateCache.CandidateVer != NULL && stateCache.Upgradable()) {
            packageState |= Upgradeable;
            if (stateCache.Keep()) {
                packageState |= Held;
            }
      }

        if (stateCache.Downgrade()) {
            packageState |= ToDowngrade;
        }
    } else {
        packageState |= NotInstalled;
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

    if (stateCache.CandidateVer == 0) {
        packageState |= NotDownloadable;
    } else if (!stateCache.CandidateVerIter(*d->depCache).Downloadable()) {
        packageState |= NotDownloadable;
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

   return packageState | d->state;
}

bool Package::isInstalled() const
{
    pkgCache::VerIterator ver = d->packageIter->CurrentVer();

    return !ver.end();
}

bool Package::isSupported() const
{
    if (origin() == "Ubuntu") {
        QString componentString = component();
        if ((componentString == QLatin1String("main") ||
             componentString == QLatin1String("restricted")) && isTrusted()) {
            return true;
        }
    }

    return false;
}

QStringList Package::dependencyList(bool useCandidateVersion) const
{
    QStringList dependsList;
    pkgCache::VerIterator current;

    if(!useCandidateVersion) {
        current = (*d->depCache)[*d->packageIter].InstVerIter(*d->depCache);
    }
    if(useCandidateVersion || current.end()) {
        current = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    }

    // no information found
    if(current.end()) {
        return dependsList;
    }

    for(pkgCache::DepIterator D = current.DependsList(); D.end() != true; ++D) {
        QString type;
        bool isOr = false;
        bool isVirtual = false;
        bool isSatisfied = false;
        QString name;
        QString version;
        QString versionCompare;

        QString finalString;

        // check target and or-depends status
        pkgCache::PkgIterator Trg = D.TargetPkg();
        if ((D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or) {
            isOr=true;
        }

        // common information
        type = QString::fromUtf8(D.DepType());
        name = Trg.Name();

        // satisfied
        if (((*d->depCache)[D] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall) {
            isSatisfied = true;
        }
        if (Trg->VersionList == 0) {
            isVirtual = true;
        } else {
            version=D.TargetVer();
            versionCompare=D.CompType();
        }

        finalString = QLatin1String("<b>") % type % QLatin1String(":</b> ");
        if (isVirtual) {
            finalString += QLatin1String("<i>") % name % QLatin1String("</i>");
        } else {
            finalString += name;
        }

        // Escape the compare operator so it won't be seen as HTML
        if (!version.isEmpty()) {
            QString compMarkup(versionCompare);
            compMarkup.replace(QLatin1Char('<'), QLatin1String("&lt;"));
            finalString += QLatin1String(" (") % compMarkup % QLatin1Char(' ') % version % QLatin1Char(')');
        }

        if (isOr) {
            finalString += QLatin1String(" |");
        }

        dependsList.append(finalString);
    }

    return dependsList;
}

QStringList Package::requiredByList() const
{
    QStringList reverseDependsList;

    for(pkgCache::DepIterator it = d->packageIter->RevDependsList(); !it.end(); ++it) {
        reverseDependsList << QString::fromStdString(it.ParentPkg().Name());
    }

    return reverseDependsList;
}

QStringList Package::providesList() const
{
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    if (State.CandidateVer == 0) {
        return QStringList();
    }

    QStringList provides;

    for (pkgCache::PrvIterator Prv =
         State.CandidateVerIter(*d->depCache).ProvidesList(); !Prv.end(); ++Prv) {
        provides.append(Prv.Name());
    }

   return provides;
}

QHash<int, QHash<QString, QVariantMap> > Package::brokenReason() const
{
    pkgCache::DepIterator depI;
    pkgCache::VerIterator Ver;
    bool First = true;

    QHash<QString, QVariantMap> notInstallable;
    QHash<QString, QVariantMap> wrongCandidate;
    QHash<QString, QVariantMap> depNotInstallable;
    QHash<QString, QVariantMap> virtualPackage;

    // failTrain represents brokenness, but also the complexity of this
    // function...
    QHash<int, QHash<QString, QVariantMap> > failTrain;

    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    Ver = State.CandidateVerIter(*d->depCache);

    // check if there is actually something to install
    if (Ver == 0) {
        QHash<QString, QVariantMap> parentNotInstallable;
        parentNotInstallable[name()] = QVariantMap();
        failTrain[QApt::ParentNotInstallable] = parentNotInstallable;
        return failTrain;
    }

    for (pkgCache::DepIterator D = Ver.DependsList(); !D.end();) {
        // Compute a single dependency element (glob or)
        pkgCache::DepIterator Start;
        pkgCache::DepIterator End;
        D.GlobOr(Start, End);

        pkgCache::PkgIterator Targ = Start.TargetPkg();

        if (!d->depCache->IsImportantDep(End)) {
            continue;
        }

        if (((*d->depCache)[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall) {
            continue;
        }

        if (Targ->ProvidesList == 0) {
            // Ok, not a virtual package since no provides
            pkgCache::VerIterator Ver =  (*d->depCache)[Targ].InstVerIter(*d->depCache);

            QString requiredVersion;
            if(Start.TargetVer() != 0) {
                requiredVersion = '(' % QString::fromStdString(Start.CompType())
                                  % QString::fromStdString(Start.TargetVer()) % ')';
            }

            if (!Ver.end()) {
                // Happens when a package needs an upgraded dep, but the dep won't
                // upgrade. Example:
                // "apt 0.5.4 but 0.5.3 is to be installed"
                QVariantMap failReason;
                failReason["Relation"] = QString::fromStdString(End.DepType());
                failReason["RequiredVersion"] = requiredVersion;
                failReason["CandidateVersion"] = QString::fromStdString(Ver.VerStr());
                if (Start != End) {
                    failReason["IsFirstOr"] = true;
                }

                QString targetName = QString::fromStdString(Start.TargetPkg().Name());
                wrongCandidate[targetName] = failReason;
            } else { // We have the package, but for some reason it won't be installed
                // In this case, the required version does not exist at all
                if ((*d->depCache)[Targ].CandidateVerIter(*d->depCache).end()) {
                    QVariantMap failReason;
                    failReason["Relation"] = QString::fromStdString(End.DepType());
                    failReason["RequiredVersion"] = requiredVersion;
                    if (Start != End) {
                        failReason["IsFirstOr"] = true;
                    }

                    QString targetName = QString::fromStdString(Start.TargetPkg().Name());
                    depNotInstallable[targetName] = failReason;
                } else {
                    // Who knows why it won't be installed? Getting here means we have no good reason
                    QVariantMap failReason;
                    failReason["Relation"] = QString::fromStdString(End.DepType());
                    if (Start != End) {
                        failReason["IsFirstOr"] = true;
                    }

                    QString targetName = QString::fromStdString(Start.TargetPkg().Name());
                    depNotInstallable[targetName] = failReason;
                }
            }
        } else {
            // Ok, candidate has provides. We're a virtual package
            QVariantMap failReason;
            failReason["Relation"] = QString::fromStdString(End.DepType());
            if (Start != End) {
                failReason["IsFirstOr"] = true;
            }

            QString targetName = QString::fromStdString(Start.TargetPkg().Name());
            virtualPackage[targetName] = failReason;
        }
    }

    failTrain[QApt::WrongCandidateVersion] = wrongCandidate;
    failTrain[QApt::DepNotInstallable] = depNotInstallable;
    failTrain[QApt::VirtualPackage] = virtualPackage;

    return failTrain;
}

bool Package::isTrusted() const
{
    pkgCache::VerIterator Ver;
    pkgDepCache::StateCache & State = (*d->depCache)[*d->packageIter];
    Ver = State.CandidateVerIter(*d->depCache);
    if (Ver == 0) {
        return false;
    }

    pkgSourceList *Sources = d->backend->packageSourceList();
    QHash<pkgCache::PkgFileIterator, pkgIndexFile*> *trustCache = d->backend->cache()->trustCache();
    for (pkgCache::VerFileIterator i = Ver.FileList(); !i.end(); ++i)
    {
        pkgIndexFile *Index;

        //FIXME: Should be done in apt
        QHash<pkgCache::PkgFileIterator, pkgIndexFile*>::const_iterator trustIter = trustCache->constBegin();
        while (trustIter != trustCache->constEnd()) {
            if (trustIter.key() == i.File()) {
                break;
            }
        }

        if (trustIter == trustCache->constEnd()) { // Not found
            if (!Sources->FindIndex(i.File(), Index)) {
              continue;
            }
        } else {
            Index = trustIter.value();
        }
        if (Index->IsTrusted()) {
            return true;
        }
    }

    return false;
}

bool Package::wouldBreak() const
{
    int pkgState = state();

    if ((pkgState & ToRemove) || (!(pkgState & Installed) && (pkgState & ToKeep))) {
        return false;
    }
    return pkgState & InstallBroken;
}

void Package::setAuto(bool flag)
{
    d->depCache->MarkAuto(*d->packageIter, flag);
}


void Package::setKeep()
{
    d->depCache->MarkKeep(*d->packageIter, false);
    pkgFixBroken(*d->depCache);
    d->backend->packageChanged(this);
}

void Package::setInstall()
{
    d->depCache->MarkInstall(*d->packageIter, true);

    // FIXME: can't we get rid of it here?
    // if there is something wrong, try to fix it
    if (!state() & ToInstall || d->depCache->BrokenCount() > 0) {
        pkgProblemResolver Fix(d->depCache);
        Fix.Clear(*d->packageIter);
        Fix.Protect(*d->packageIter);
        Fix.Resolve(true);
    }

    d->backend->packageChanged(this);
}

void Package::setReInstall()
{
    d->depCache->SetReInstall(*d->packageIter, true);
    d->backend->packageChanged(this);
}


void Package::setRemove()
{
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

bool Package::setVersion(const QString &version)
{
    QString defaultCandVer;
    pkgDepCache::StateCache &state = (*d->depCache)[*d->packageIter];
    if (state.CandVersion != NULL) {
        defaultCandVer = state.CandVersion;
    }

    bool isDefault = (version == defaultCandVer);
    pkgVersionMatch Match(version.toStdString(), pkgVersionMatch::Version);
    pkgCache::VerIterator Ver = Match.Find(*d->packageIter);

    if (Ver.end()) {
        return false;
    }

    d->depCache->SetCandidateVersion(Ver);

    if (isDefault) {
        d->state &= ~OverrideVersion;
    } else {
        d->state |= OverrideVersion;
    }

    return true;
}

}
