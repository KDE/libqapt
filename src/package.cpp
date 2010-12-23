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
        ~PackagePrivate() { delete packageIter; };
        QApt::Backend *backend;
        pkgDepCache *depCache;
        pkgRecords *records;
        pkgCache::PkgIterator *packageIter;
        int state;

        pkgCache::PkgFileIterator searchPkgFileIter(const QLatin1String &label, const QString &release) const;
        QString getReleaseFileForOrigin(const QLatin1String &label, const QString &release) const;
};

pkgCache::PkgFileIterator PackagePrivate::searchPkgFileIter(const QLatin1String &label, const QString &release) const
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
                    if(verLabel == label && verOrigin == label &&
                       QLatin1String(verArchive) == release) {
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

QString PackagePrivate::getReleaseFileForOrigin(const QLatin1String &label, const QString &release) const
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

                QString uri = QLatin1String(stduri.c_str());
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
    // We have to make our own pkgIterator, since the one passed here will
    // keep on iterating while all the packages are being built
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
    QString name = latin1Name();

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
    QString section = latin1Section();

    return section;
}

QLatin1String Package::latin1Section() const
{
    QLatin1String section(d->packageIter->Section());

    return section;
}

QString Package::sourcePackage() const
{
    QString sourcePackage;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser &rec = d->records->Lookup(ver.FileList());
        sourcePackage = QLatin1String(rec.SourcePkg().c_str());
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
        QStringList sections = rawDescription.split(QLatin1String("\n ."));

        int i;
        for (i = 0; i < sections.count(); ++i) {
            sections[i].replace(QRegExp(QLatin1String("\n( |\t)+(-|\\*)")),
                                QLatin1Literal("\n\r ") % QString::fromUtf8("\xE2\x80\xA2"));
            // There should be no new lines within a section.
            sections[i].remove(QLatin1Char('\n'));
            // Hack to get the lists working again.
            sections[i].replace(QLatin1Char('\r'), QLatin1Char('\n'));
            // Merge multiple whitespace chars into one
            sections[i].replace(QRegExp(QLatin1String("\\ \\ +")), QChar::fromLatin1(' '));
            // Remove the initial whitespace
            sections[i].remove(0, 1);
            // Append to parsedDescription
            if (sections[i].startsWith(QLatin1String("\n ") % QString::fromUtf8("\xE2\x80\xA2 ")) || !i) {
                parsedDescription += sections[i];
            }  else {
                parsedDescription += QLatin1Literal("\n\n") % sections[i];
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
        pkgRecords::Parser &parser = d->records->Lookup(ver.FileList());
        maintainer = QString::fromUtf8(parser.Maintainer().data());
        maintainer.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    }
    return maintainer;
}

QString Package::homepage() const
{
    QString homepage;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!ver.end()) {
        pkgRecords::Parser &parser = d->records->Lookup(ver.FileList());
        homepage = QString::fromUtf8(parser.Homepage().data());
    }
    return homepage;
}

QString Package::version() const
{
    if (!(*d->packageIter)->CurrentVer) {
        pkgDepCache::StateCache &State = (*d->depCache)[*d->packageIter];
        if (!State.CandidateVer) {
            return QString();
        } else {
            return QLatin1String(State.CandidateVerIter(*d->depCache).VerStr());
        }
    } else {
        return QLatin1String(d->packageIter->CurrentVer().VerStr());
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
                versions.append(QLatin1String(Ver.VerStr()) % QLatin1Literal(" (") %
                QLatin1String(File.Archive()) % QLatin1Char(')'));
            } else {
                versions.append(QLatin1String(Ver.VerStr()) % QLatin1Literal(" (") %
                QLatin1String(File.Site()) % QLatin1Char(')'));
            }
        }
    }

    return versions;
}

QString Package::installedVersion() const
{
    if (!(*d->packageIter)->CurrentVer) {
        return QString();
    }
    QString installedVersion = QLatin1String(d->packageIter->CurrentVer().VerStr());
    return installedVersion;
}

QString Package::availableVersion() const
{
    pkgDepCache::StateCache &State = (*d->depCache)[*d->packageIter];
    if (!State.CandidateVer) {
        return QString();
    }

    QString availableVersion = QLatin1String(State.CandidateVerIter(*d->depCache).VerStr());
    return availableVersion;
}

QString Package::priority() const
{
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (ver != 0) {
        QString priority = QLatin1String(ver.PriorityType());
        return priority;
    } else {
        return QString();
    }
}

QStringList Package::installedFilesList() const
{
    QStringList installedFilesList;
    QFile infoFile(QLatin1Literal("/var/lib/dpkg/info/") % name() % QLatin1Literal(".list"));

    if (infoFile.open(QFile::ReadOnly)) {
        QTextStream stream(&infoFile);
        QString line;

        do {
            line = stream.readLine();
            installedFilesList << line;
        } while (!line.isNull());

        // The first item won't be a file
        installedFilesList.removeFirst();

        // Remove non-file directory listings
        for (int i = 0; i < installedFilesList.size() - 1; ++i) {
            if (installedFilesList.at(i+1).contains(installedFilesList.at(i))) {
                installedFilesList[i] = QString(QLatin1Char(' '));
            }
        }

        installedFilesList.removeAll(QChar::fromLatin1(' '));
        // Last line is empty for some reason...

        if (!installedFilesList.isEmpty()) {
            installedFilesList.removeLast();
        }
    }

    return installedFilesList;
}

QString Package::origin() const
{
    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    if(!Ver.end()) {
         pkgCache::VerFileIterator VF = Ver.FileList();
         return QLatin1String(VF.File().Origin());
    }

    return QString();
}

QString Package::component() const
{
    pkgDepCache::StateCache &State = (*d->depCache)[*d->packageIter];
    if (!State.CandidateVer) {
        return QString();
    }
    pkgCache::VerIterator Ver = State.CandidateVerIter(*d->depCache);
    pkgCache::VerFileIterator VF = Ver.FileList();
    pkgCache::PkgFileIterator File = VF.File();

    if(File.Component() == NULL) {
        return QString();
    }

    return QLatin1String(File.Component());
}

QUrl Package::changelogUrl() const
{
    QString prefix;
    const QString srcPackage = sourcePackage();
    QString sourceSection = section();

    if (sourceSection.contains(QLatin1Char('/'))) {
        QStringList split = sourceSection.split(QLatin1Char('/'));
        sourceSection = split.at(0);
    } else {
        sourceSection = QLatin1String("main");
    }

    if (srcPackage.size() > 3 && srcPackage.startsWith(QLatin1String("lib"))) {
        prefix = QLatin1Literal("lib") % srcPackage[3];
    } else {
        prefix = srcPackage[0];
    }

    QString versionString;
    if (!availableVersion().isEmpty()) {
        versionString = availableVersion();
    }

    if (versionString.contains(QLatin1Char(':'))) {
        QStringList epochVersion = versionString.split(QLatin1Char(':'));
        // If the version has an epoch, take the stuff after the epoch
        versionString = epochVersion[1];
    }

    QString urlBase = QLatin1String("http://changelogs.ubuntu.com/changelogs/pool/");
    QUrl url = QUrl(urlBase % sourceSection % QLatin1Char('/') % prefix % QLatin1Char('/') %
                    srcPackage % QLatin1Char('/') % srcPackage % QLatin1Char('_') % versionString % '/'
                    % QLatin1Literal("changelog"));

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

    QUrl url = QUrl(urlBase % latin1Name());

    return url;
}

QString Package::supportedUntil() const
{
    if (!isSupported()) {
        return QString();
    }

    QFile lsb_release(QLatin1String("/etc/lsb-release"));
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
        QStringList split = line.split(QLatin1Char('='));
        if (split[0] == QLatin1String("DISTRIB_CODENAME")) {
            release = split[1];
        }
    } while (!line.isNull());

    // Canonical only provides support for Ubuntu, but we don't have to worry
    // about Debian systems as long as we assume that this function can fail.
    QString releaseFile = d->getReleaseFileForOrigin(QLatin1String("Ubuntu"), release);

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

    QString supportTimeString = controlField(QLatin1String("Supported"));
    supportTimeString.chop(1); // Remove the letter signifying months
    const int supportTime = supportTimeString.toInt(); // months

    QDateTime supportEnd = QDateTime::fromTime_t(releaseDate).addMonths(supportTime);

    return supportEnd.toString(QLatin1String("MMMM yyyy"));
}

QString Package::controlField(const QLatin1String &name) const
{
    QString field;
    pkgCache::VerIterator ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (ver.end()) {
        return field;
    }

    pkgRecords::Parser &rec = d->records->Lookup(ver.FileList());
    const char *start, *stop;
    rec.GetRec(start, stop);
    QString record(start);

    QStringList lines = record.split(QLatin1Char('\n'));

    foreach (const QString &line, lines) {
        if (line.startsWith(name)) {
            field = line.split(QLatin1String(": ")).at(1);
            break;
        }
    }

    return field;
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
    pkgDepCache::StateCache &State = (*d->depCache)[*d->packageIter];
    if (!State.CandidateVer) {
        return qint64(-1);
    }
    return qint64(State.CandidateVerIter(*d->depCache)->InstalledSize);
}

qint64 Package::downloadSize() const
{
    pkgDepCache::StateCache &State = (*d->depCache)[*d->packageIter];
    if (!State.CandidateVer) {
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

    if (!stateCache.CandidateVer) {
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
    if (origin() == QLatin1String("Ubuntu")) {
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
    pkgDepCache::StateCache &State = (*d->depCache)[*d->packageIter];

    if(!useCandidateVersion) {
        current = State.InstVerIter(*d->depCache);
    }
    if(useCandidateVersion || current.end()) {
        current = State.CandidateVerIter(*d->depCache);
    }

    // no information found
    if(current.end()) {
        return dependsList;
    }

    for(pkgCache::DepIterator D = current.DependsList(); D.end() != true; ++D) {
        QString type;
        bool isOr = false;
        bool isVirtual = false;
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
        name = QLatin1String(Trg.Name());

        if (!Trg->VersionList) {
            isVirtual = true;
        } else {
            version = QLatin1String(D.TargetVer());
            versionCompare = QLatin1String(D.CompType());
        }

        finalString = QLatin1Literal("<b>") % type % QLatin1Literal(":</b> ");
        if (isVirtual) {
            finalString += QLatin1Literal("<i>") % name % QLatin1Literal("</i>");
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
        reverseDependsList << QLatin1String(it.ParentPkg().Name());
    }

    return reverseDependsList;
}

QStringList Package::providesList() const
{
    pkgDepCache::StateCache &State = (*d->depCache)[*d->packageIter];
    if (!State.CandidateVer) {
        return QStringList();
    }

    QStringList provides;

    for (pkgCache::PrvIterator Prv =
         State.CandidateVerIter(*d->depCache).ProvidesList(); !Prv.end(); ++Prv) {
        provides.append(QLatin1String(Prv.Name()));
    }

   return provides;
}

QStringList Package::recommendsList() const
{
    QStringList recommends;

    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    if (Ver.end()) {
        return recommends;
    }

    for(pkgCache::DepIterator it = Ver.DependsList(); !it.end(); ++it) {
        pkgCache::PkgIterator pkg = it.TargetPkg();

        // Skip purely virtual packages
        if (!pkg->VersionList) {
            continue;
        }
        pkgDepCache::StateCache &rState = (*d->depCache)[pkg];
        if (it->Type == pkgCache::Dep::Recommends && (rState.CandidateVer != 0 )) {
            recommends << QLatin1String(it.TargetPkg().Name());
        }
    }

    return recommends;
}

QStringList Package::suggestsList() const
{
    QStringList suggests;

    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    if (Ver.end()) {
        return suggests;
    }

    for(pkgCache::DepIterator it = Ver.DependsList(); !it.end(); ++it) {
        pkgCache::PkgIterator pkg = it.TargetPkg();

        // Skip purely virtual packages
        if (!pkg->VersionList) {
            continue;
        }
        pkgDepCache::StateCache &sState = (*d->depCache)[pkg];
        if (it->Type == pkgCache::Dep::Suggests && (sState.CandidateVer != 0 )) {
            suggests << QLatin1String(it.TargetPkg().Name());
        }
    }

    return suggests;
}

QStringList Package::enhancesList() const
{
    QStringList enhances;

    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    if (Ver.end()) {
        return enhances;
    }

    for(pkgCache::DepIterator it = Ver.DependsList(); !it.end(); ++it) {
        pkgCache::PkgIterator pkg = it.TargetPkg();

        // Skip purely virtual packages
        if (!pkg->VersionList) {
            continue;
        }
        pkgDepCache::StateCache &eState = (*d->depCache)[pkg];
        if (it->Type == pkgCache::Dep::Enhances && (eState.CandidateVer != 0 )) {
            enhances << QLatin1String(it.TargetPkg().Name());
        }
    }

    return enhances;
}

QStringList Package::enhancedByList() const
{
    QStringList enhancedByList;

    foreach (QApt::Package *package, d->backend->availablePackages()) {
        if (package->enhancesList().contains(latin1Name())) {
            enhancedByList << package->latin1Name();
        }
    }

    return enhancedByList;
}


QHash<int, QHash<QString, QVariantMap> > Package::brokenReason() const
{
    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);

    QHash<QString, QVariantMap> notInstallable;
    QHash<QString, QVariantMap> wrongCandidate;
    QHash<QString, QVariantMap> depNotInstallable;
    QHash<QString, QVariantMap> virtualPackage;

    // failTrain represents brokenness, but also the complexity of this
    // function...
    QHash<int, QHash<QString, QVariantMap> > failTrain;

    // check if there is actually something to install
    if (!Ver) {
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

        if (!Targ->ProvidesList) {
            // Ok, not a virtual package since no provides
            pkgCache::VerIterator Ver =  (*d->depCache)[Targ].InstVerIter(*d->depCache);

            QString requiredVersion;
            if(Start.TargetVer() != 0) {
                requiredVersion = '(' % QLatin1String(Start.CompType())
                                  % QLatin1String(Start.TargetVer()) % ')';
            }

            if (!Ver.end()) {
                // Happens when a package needs an upgraded dep, but the dep won't
                // upgrade. Example:
                // "apt 0.5.4 but 0.5.3 is to be installed"
                QVariantMap failReason;
                failReason[QLatin1String("Relation")] = QLatin1String(End.DepType());
                failReason[QLatin1String("RequiredVersion")] = requiredVersion;
                failReason[QLatin1String("CandidateVersion")] = QLatin1String(Ver.VerStr());
                if (Start != End) {
                    failReason[QLatin1String("IsFirstOr")] = true;
                }

                QString targetName = QLatin1String(Start.TargetPkg().Name());
                wrongCandidate[targetName] = failReason;
            } else { // We have the package, but for some reason it won't be installed
                // In this case, the required version does not exist at all
                if ((*d->depCache)[Targ].CandidateVerIter(*d->depCache).end()) {
                    QVariantMap failReason;
                    failReason[QLatin1String("Relation")] = QLatin1String(End.DepType());
                    failReason[QLatin1String("RequiredVersion")] = requiredVersion;
                    if (Start != End) {
                        failReason[QLatin1String("IsFirstOr")] = true;
                    }

                    QString targetName = QLatin1String(Start.TargetPkg().Name());
                    depNotInstallable[targetName] = failReason;
                } else {
                    // Who knows why it won't be installed? Getting here means we have no good reason
                    QVariantMap failReason;
                    failReason[QLatin1String("Relation")] = QLatin1String(End.DepType());
                    if (Start != End) {
                        failReason[QLatin1String("IsFirstOr")] = true;
                    }

                    QString targetName = QLatin1String(Start.TargetPkg().Name());
                    depNotInstallable[targetName] = failReason;
                }
            }
        } else {
            // Ok, candidate has provides. We're a virtual package
            QVariantMap failReason;
            failReason[QLatin1String("Relation")] = QLatin1String(End.DepType());
            if (Start != End) {
                failReason[QLatin1String("IsFirstOr")] = true;
            }

            QString targetName = QLatin1String(Start.TargetPkg().Name());
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
    pkgCache::VerIterator Ver = (*d->depCache)[*d->packageIter].CandidateVerIter(*d->depCache);
    if (!Ver) {
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
    QLatin1String defaultCandVer("");
    pkgDepCache::StateCache &state = (*d->depCache)[*d->packageIter];
    if (state.CandVersion != NULL) {
        defaultCandVer = QLatin1String(state.CandVersion);
    }

    bool isDefault = (version == defaultCandVer);
    pkgVersionMatch Match(version.toLatin1().constData(), pkgVersionMatch::Version);
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
