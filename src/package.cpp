/***************************************************************************
 *   Copyright © 2010-2011 Jonathan Thomas <echidnaman@kubuntu.org>        *
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
#include <apt-pkg/debversion.h>
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
#include "config.h"

namespace QApt {

class PackagePrivate
{
    public:
        PackagePrivate()
            : state(0)
            , staticStateCalculated(false)
            , foreignArchCalculated(false)
        {
        }
        ~PackagePrivate()
        {
            delete packageIter;
        }
        QApt::Backend *backend;
        pkgCache::PkgIterator *packageIter;
        int state;
        bool staticStateCalculated;
        bool isForeignArch;
        bool foreignArchCalculated;

        pkgCache::PkgFileIterator searchPkgFileIter(QLatin1String label, const QString &release) const;
        QString getReleaseFileForOrigin(QLatin1String label, const QString &release) const;

        // Calculate state flags that cannot change
        void initStaticState(const pkgCache::VerIterator &ver, pkgDepCache::StateCache &stateCache);
};

pkgCache::PkgFileIterator PackagePrivate::searchPkgFileIter(QLatin1String label, const QString &release) const
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

QString PackagePrivate::getReleaseFileForOrigin(QLatin1String label, const QString &release) const
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
                QString uri = backend->config()->findDirectory("Dir::State::lists")
                % QString::fromStdString(URItoFileName((*I)->GetURI()))
                % QLatin1String("dists_")
                % QString::fromStdString((*I)->GetDist())
                % QLatin1String("_Release");

                return uri;
            }
        }
    }

    return QString();
}

void PackagePrivate::initStaticState(const pkgCache::VerIterator &ver, pkgDepCache::StateCache &stateCache)
{
    int packageState = 0;

    if (!ver.end()) {
        packageState |= QApt::Package::Installed;

        if (stateCache.CandidateVer && stateCache.Upgradable()) {
            packageState |= QApt::Package::Upgradeable;
            if (stateCache.Keep()) {
                packageState |= QApt::Package::Held;
            }
        }

        if (stateCache.Downgrade()) {
            packageState |= QApt::Package::ToDowngrade;
        }
    } else {
        packageState |= QApt::Package::NotInstalled;
    }
    if (stateCache.NowBroken()) {
        packageState |= QApt::Package::NowBroken;
    }

    if (stateCache.InstBroken()) {
        packageState |= QApt::Package::InstallBroken;
    }

    if ((*packageIter)->Flags & (pkgCache::Flag::Important |
                                 pkgCache::Flag::Essential)) {
        packageState |= QApt::Package::IsImportant;
    }

    if ((*packageIter)->CurrentState == pkgCache::State::ConfigFiles) {
        packageState |= QApt::Package::ResidualConfig;
    }

    if (!stateCache.CandidateVer) {
        packageState |= QApt::Package::NotDownloadable;
    } else if (!stateCache.CandidateVerIter(*backend->cache()->depCache()).Downloadable()) {
        packageState |= QApt::Package::NotDownloadable;
    }

    if (stateCache.Flags & pkgCache::Flag::Auto) {
        packageState |= QApt::Package::IsAuto;
    }

    if (stateCache.Garbage) {
        packageState |= QApt::Package::IsGarbage;
    }

    if (stateCache.NowPolicyBroken()) {
        packageState |= QApt::Package::NowPolicyBroken;
    }

    if (stateCache.InstPolicyBroken()) {
        packageState |= QApt::Package::InstallPolicyBroken;
    }

    state |= packageState;

    staticStateCalculated = true;
}

Package::Package(QApt::Backend* backend, pkgDepCache *depCache,
                 pkgRecords *records, pkgCache::PkgIterator &packageIter)
        : d(new PackagePrivate())
{
    Q_UNUSED(depCache)
    Q_UNUSED(records)
    // We have to make our own pkgIterator, since the one passed here will
    // keep on iterating while all the packages are being built
    d->packageIter = new pkgCache::PkgIterator(packageIter);
    d->backend = backend;
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
    return latin1Name();
}

QLatin1String Package::latin1Name() const
{
    return QLatin1String(d->packageIter->Name());
}

int Package::id() const
{
    return (*d->packageIter)->ID;
}

QString Package::section() const
{
    return latin1Section();
}

QLatin1String Package::latin1Section() const
{
    return QLatin1String(d->packageIter->Section());
}

QString Package::sourcePackage() const
{
    QString sourcePackage;
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (!ver.end()) {
        pkgRecords::Parser &rec = d->backend->records()->Lookup(ver.FileList());
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
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (!ver.end()) {
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->backend->records()->Lookup(Desc.FileList());
        shortDescription = QString::fromUtf8(parser.ShortDesc().data());
        return shortDescription;
    }

    return shortDescription;
}

QString Package::longDescription() const
{
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

    if (!ver.end()) {
        QString rawDescription;
        pkgCache::DescIterator Desc = ver.TranslatedDescription();
        pkgRecords::Parser & parser = d->backend->records()->Lookup(Desc.FileList());
        rawDescription = QString::fromUtf8(parser.LongDesc().data());
        // Apt acutally returns the whole description, we just want the
        // extended part.
        rawDescription.remove(shortDescription() % '\n');
        // *Now* we're really raw. Sort of. ;)

        QString parsedDescription;
        // Split at double newline, by "section"
        QStringList sections = rawDescription.split(QLatin1String("\n ."));

        for (int i = 0; i < sections.count(); ++i) {
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
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (!ver.end()) {
        pkgRecords::Parser &parser = d->backend->records()->Lookup(ver.FileList());
        maintainer = QString::fromUtf8(parser.Maintainer().data());
        maintainer.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    }
    return maintainer;
}

QString Package::homepage() const
{
    QString homepage;
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (!ver.end()) {
        pkgRecords::Parser &parser = d->backend->records()->Lookup(ver.FileList());
        homepage = QString::fromUtf8(parser.Homepage().data());
    }
    return homepage;
}

QString Package::version() const
{
    if (!(*d->packageIter)->CurrentVer) {
        pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[*d->packageIter];
        if (!State.CandidateVer) {
            return QString();
        } else {
            return QLatin1String(State.CandidateVerIter(*d->backend->cache()->depCache()).VerStr());
        }
    } else {
        return QLatin1String(d->packageIter->CurrentVer().VerStr());
    }
}

QString Package::upstreamVersion() const
{
    const char *ver;

    if (!(*d->packageIter)->CurrentVer) {
        pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[*d->packageIter];
        if (!State.CandidateVer) {
            return QString();
        } else {
            ver = State.CandidateVerIter(*d->backend->cache()->depCache()).VerStr();
        }
    } else {
        ver = d->packageIter->CurrentVer().VerStr();
    }

    return QString::fromStdString(_system->VS->UpstreamVersion(ver));
}

QString Package::upstreamVersion(const QString &version)
{
    return QString::fromStdString(_system->VS->UpstreamVersion(version.toStdString().c_str()));
}

QString Package::architecture() const
{
    pkgDepCache *depCache = d->backend->cache()->depCache();
    pkgCache::VerIterator ver = (*depCache)[*d->packageIter].InstVerIter(*depCache);

    // the arch:all property is part of the version
    if (ver && ver.Arch())
        return ver.Arch();

    return d->packageIter->Arch();
}

QStringList Package::availableVersions() const
{
    QStringList versions;

    // Get available Versions.
    for (pkgCache::VerIterator Ver = d->packageIter->VersionList(); !Ver.end(); ++Ver) {

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

    return QLatin1String(d->packageIter->CurrentVer().VerStr());
}

QString Package::availableVersion() const
{
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[*d->packageIter];
    if (!State.CandidateVer) {
        return QString();
    }

    return QLatin1String(State.CandidateVerIter(*d->backend->cache()->depCache()).VerStr());
}

QString Package::priority() const
{
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (!ver.end()) {
        return QLatin1String(ver.PriorityType());
    }

    return QString();
}

QStringList Package::installedFilesList() const
{
    QStringList installedFilesList;
    QString path = QLatin1String("/var/lib/dpkg/info/") % name() % QLatin1String(".list");

    // Fallback for multiarch packages
    if (!QFile::exists(path)) {
        path = QLatin1String("/var/lib/dpkg/info/") % name() % ':' %
                             architecture() % QLatin1String(".list");
    }

    QFile infoFile(path);

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
            if (installedFilesList.at(i+1).contains(installedFilesList.at(i) + '/')) {
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
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

    if(!Ver.end()) {
         pkgCache::VerFileIterator VF = Ver.FileList();
         return QLatin1String(VF.File().Origin());
    }

    return QString();
}

QStringList Package::archives() const
{
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

    if(!Ver.end()) {
        QStringList archiveList;
        for (pkgCache::VerFileIterator VF = Ver.FileList(); !VF.end(); ++VF) {
            archiveList << QLatin1String(VF.File().Archive());
        }
            return archiveList;
    }

    return QStringList();
}

QString Package::component() const
{
    QString section = latin1Section();
    if(section.isEmpty())
        return QString();

    QStringList split = section.split('/');

    if (split.count())
        return split.first();

    return QString("main");
}

QByteArray Package::md5Sum() const
{
    QByteArray md5Sum;

    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

    if(ver.end()) {
        return md5Sum;
    }

    pkgRecords::Parser &rec = d->backend->records()->Lookup(ver.FileList());
    md5Sum = rec.MD5Hash().c_str();

    return md5Sum;

}

QUrl Package::changelogUrl() const
{
    QUrl url;

    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (ver.end()) {
        return url;
    }

    pkgRecords::Parser &rec = d->backend->records()->Lookup(ver.FileList());

    QString path = QLatin1String(rec.FileName().c_str());
    path = path.left(path.lastIndexOf(QLatin1Char('/')) + 1);

    QString versionString;
    if (!availableVersion().isEmpty()) {
        versionString = availableVersion();
    }

    if (versionString.contains(QLatin1Char(':'))) {
        QStringList epochVersion = versionString.split(QLatin1Char(':'));
        // If the version has an epoch, take the stuff after the epoch
        versionString = epochVersion[1];
    }

    path += sourcePackage() % QLatin1Char('_') % versionString % QLatin1Char('/');

    Config *config = d->backend->config();
    QString server = config->readEntry(QLatin1String("Apt::Changelogs::Server"),
                                       QLatin1String("http://packages.debian.org/changelogs"));

    if(!server.contains(QLatin1String("debian"))) {
        url = QUrl(server % QLatin1Char('/') % path % QLatin1Literal("changelog"));
    } else {
        // Debian servers use changelog.txt
        url = QUrl(server % QLatin1Char('/') % path % QLatin1Literal("changelog.txt"));
    }

    return url;
}

QUrl Package::screenshotUrl(QApt::ScreenshotType type) const
{
    QUrl url;
    switch (type) {
        case QApt::Thumbnail:
            url = QUrl(controlField(QLatin1String("Thumbnail-Url")));
            if(url.isEmpty())
                url = QUrl("http://screenshots.debian.net/thumbnail/" % latin1Name());
            break;
        case QApt::Screenshot:
            url = QUrl(controlField(QLatin1String("Screenshot-Url")));
            if(url.isEmpty())
                url = QUrl("http://screenshots.debian.net/screenshot/" % latin1Name());
            break;
    }

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
        if (split.size() != 2) {
            continue;
        }

        if (split.at(0) == QLatin1String("DISTRIB_CODENAME")) {
            release = split.at(1);
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

    // Default to 18m in case the package has no "supported" field
    QString supportTimeString = QLatin1String("18m");
    QString supportTimeField = controlField(QLatin1String("Supported"));

    if (!supportTimeField.isEmpty()) {
        supportTimeString = supportTimeField;
    }

    QChar unit = supportTimeString.at(supportTimeString.length() - 1);
    supportTimeString.chop(1); // Remove the letter signifying months/years
    const int supportTime = supportTimeString.toInt();

    QDateTime supportEnd;

    if (unit == QLatin1Char('m')) {
        supportEnd = QDateTime::fromTime_t(releaseDate).addMonths(supportTime);
    } else if (unit == QLatin1Char('y')) {
        supportEnd = QDateTime::fromTime_t(releaseDate).addYears(supportTime);
    }

    return supportEnd.toString(QLatin1String("MMMM yyyy"));
}

QString Package::controlField(const QLatin1String &name) const
{
    QString field;
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (ver.end()) {
        return field;
    }

    pkgRecords::Parser &rec = d->backend->records()->Lookup(ver.FileList());
    const char *start, *stop;
    rec.GetRec(start, stop);

    pkgTagSection sec;

    if(!sec.Scan(start, stop-start+1)) {
        return field;
    }

    field = QString::fromStdString(sec.FindS(name.latin1()));

    // Can replace the above with this if my APT patch gets accepted
    // field = QString::fromStdString(rec.RecordField(name.latin1()));

    return field;
}

QString Package::controlField(const QString &name) const
{
    return controlField(QLatin1String(name.toStdString().c_str()));
}

qint64 Package::currentInstalledSize() const
{
    const pkgCache::VerIterator &ver = d->packageIter->CurrentVer();

    if (!ver.end()) {
        return qint64(ver->InstalledSize);
    } else {
        return qint64(-1);
    }
}

qint64 Package::availableInstalledSize() const
{
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[*d->packageIter];
    if (!State.CandidateVer) {
        return qint64(-1);
    }
    return qint64(State.CandidateVerIter(*d->backend->cache()->depCache())->InstalledSize);
}

qint64 Package::downloadSize() const
{
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[*d->packageIter];
    if (!State.CandidateVer) {
        return qint64(-1);
    }

    return qint64(State.CandidateVerIter(*d->backend->cache()->depCache())->Size);
}

int Package::state() const
{
    int packageState = 0;

    const pkgCache::VerIterator &ver = d->packageIter->CurrentVer();
    pkgDepCache::StateCache &stateCache = (*d->backend->cache()->depCache())[*d->packageIter];

    if (!d->staticStateCalculated) {
        d->initStaticState(ver, stateCache);
    }

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

   return packageState | d->state;
}

int Package::compareVersion(const QString &v1, const QString &v2)
{
    // Make deep copies of toStdString(), since otherwise they would
    // go out of scope when we call c_str()
    string s1 = v1.toStdString();
    string s2 = v2.toStdString();

    const char *a = s1.c_str();
    const char *b = s2.c_str();
    int lenA = strlen(a);
    int lenB = strlen(b);

    return _system->VS->DoCmpVersion(a, a+lenA, b, b+lenB);
}

bool Package::isInstalled() const
{
    return !d->packageIter->CurrentVer().end();
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

bool Package::isMultiArchEnabled() const
{
    return isForeignArch();
}

bool Package::isMultiArchDuplicate() const
{
    // Excludes installed packages, which are always "interesting"
    if (isInstalled())
        return false;

    // Otherwise, check if the pkgIterator is the "best" from its group
    return (d->packageIter->Group().FindPkg() != *d->packageIter);
}

QString Package::multiArchTypeString() const
{
    return controlField(QLatin1String("Multi-Arch"));
}

MultiArchType Package::multiArchType() const
{
    QString typeString = multiArchTypeString();
    MultiArchType archType = InvalidMultiArchType;

    if (typeString == QLatin1String("same"))
        archType = MultiArchSame;
    else if (typeString == QLatin1String("foreign"))
        archType = MultiArchForeign;
    else if (typeString == QLatin1String("allowed"))
        archType = MultiArchAllowed;

    return archType;
}

bool Package::isForeignArch() const
{
    if (!d->foreignArchCalculated) {
        QString arch = architecture();
        d->isForeignArch = (d->backend->nativeArchitecture() != arch) & (arch != QLatin1String("all"));
        d->foreignArchCalculated = true;
    }

    return d->isForeignArch;
}

QList<DependencyItem> Package::depends() const
{
    return DependencyInfo::parseDepends(controlField("Depends"), Depends);
}

QList<DependencyItem> Package::preDepends() const
{
    return DependencyInfo::parseDepends(controlField("Pre-Depends"), PreDepends);
}

QList<DependencyItem> Package::suggests() const
{
    return DependencyInfo::parseDepends(controlField("Suggests"), Suggests);
}

QList<DependencyItem> Package::recommends() const
{
    return DependencyInfo::parseDepends(controlField("Recommends"), Recommends);
}

QList<DependencyItem> Package::conflicts() const
{
    return DependencyInfo::parseDepends(controlField("Conflicts"), Conflicts);
}

QList<DependencyItem> Package::replaces() const
{
    return DependencyInfo::parseDepends(controlField("Replaces"), Replaces);
}

QList<DependencyItem> Package::obsoletes() const
{
    return DependencyInfo::parseDepends(controlField("Obsoletes"), Obsoletes);
}

QList<DependencyItem> Package::breaks() const
{
    return DependencyInfo::parseDepends(controlField("Breaks"), Breaks);
}

QList<DependencyItem> Package::enhances() const
{
    return DependencyInfo::parseDepends(controlField("Enhance"), Enhances);
}

QStringList Package::dependencyList(bool useCandidateVersion) const
{
    QStringList dependsList;
    pkgCache::VerIterator current;
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[*d->packageIter];

    if(!useCandidateVersion) {
        current = State.InstVerIter(*d->backend->cache()->depCache());
    }
    if(useCandidateVersion || current.end()) {
        current = State.CandidateVerIter(*d->backend->cache()->depCache());
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
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[*d->packageIter];
    if (!State.CandidateVer) {
        return QStringList();
    }

    QStringList provides;

    for (pkgCache::PrvIterator Prv =
         State.CandidateVerIter(*d->backend->cache()->depCache()).ProvidesList(); !Prv.end(); ++Prv) {
        provides.append(QLatin1String(Prv.Name()));
    }

   return provides;
}

QStringList Package::recommendsList() const
{
    QStringList recommends;

    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

    if (Ver.end()) {
        return recommends;
    }

    for(pkgCache::DepIterator it = Ver.DependsList(); !it.end(); ++it) {
        pkgCache::PkgIterator pkg = it.TargetPkg();

        // Skip purely virtual packages
        if (!pkg->VersionList) {
            continue;
        }
        pkgDepCache::StateCache &rState = (*d->backend->cache()->depCache())[pkg];
        if (it->Type == pkgCache::Dep::Recommends && (rState.CandidateVer != 0 )) {
            recommends << QLatin1String(it.TargetPkg().Name());
        }
    }

    return recommends;
}

QStringList Package::suggestsList() const
{
    QStringList suggests;

    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

    if (Ver.end()) {
        return suggests;
    }

    for(pkgCache::DepIterator it = Ver.DependsList(); !it.end(); ++it) {
        pkgCache::PkgIterator pkg = it.TargetPkg();

        // Skip purely virtual packages
        if (!pkg->VersionList) {
            continue;
        }
        pkgDepCache::StateCache &sState = (*d->backend->cache()->depCache())[pkg];
        if (it->Type == pkgCache::Dep::Suggests && (sState.CandidateVer != 0 )) {
            suggests << QLatin1String(it.TargetPkg().Name());
        }
    }

    return suggests;
}

QStringList Package::enhancesList() const
{
    QStringList enhances;

    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

    if (Ver.end()) {
        return enhances;
    }

    for(pkgCache::DepIterator it = Ver.DependsList(); !it.end(); ++it) {
        pkgCache::PkgIterator pkg = it.TargetPkg();

        // Skip purely virtual packages
        if (!pkg->VersionList) {
            continue;
        }
        pkgDepCache::StateCache &eState = (*d->backend->cache()->depCache())[pkg];
        if (it->Type == pkgCache::Dep::Enhances && (eState.CandidateVer != 0 )) {
            enhances << QLatin1String(it.TargetPkg().Name());
        }
    }

    return enhances;
}

QStringList Package::enhancedByList() const
{
    QStringList enhancedByList;

    Q_FOREACH (QApt::Package *package, d->backend->availablePackages()) {
        if (package->enhancesList().contains(latin1Name())) {
            enhancedByList << package->latin1Name();
        }
    }

    return enhancedByList;
}


QHash<int, QHash<QString, QVariantMap> > Package::brokenReason() const
{
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);

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

        if (!d->backend->cache()->depCache()->IsImportantDep(End)) {
            continue;
        }

        if (((*d->backend->cache()->depCache())[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall) {
            continue;
        }

        if (!Targ->ProvidesList) {
            // Ok, not a virtual package since no provides
            pkgCache::VerIterator Ver =  (*d->backend->cache()->depCache())[Targ].InstVerIter(*d->backend->cache()->depCache());

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
                if ((*d->backend->cache()->depCache())[Targ].CandidateVerIter(*d->backend->cache()->depCache()).end()) {
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
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(*d->packageIter);
    if (!Ver) {
        return false;
    }

    pkgSourceList *Sources = d->backend->packageSourceList();
    QHash<pkgCache::PkgFileIterator, pkgIndexFile*> *trustCache = d->backend->cache()->trustCache();

    for (pkgCache::VerFileIterator i = Ver.FileList(); !i.end(); ++i)
    {
        pkgIndexFile *Index;

        //FIXME: Should be done in apt
        auto trustIter = trustCache->constBegin();
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
    d->backend->cache()->depCache()->MarkAuto(*d->packageIter, flag);
}


void Package::setKeep()
{
    d->backend->cache()->depCache()->MarkKeep(*d->packageIter, false);
    if (d->backend->cache()->depCache()->BrokenCount() > 0) {
        pkgProblemResolver Fix(d->backend->cache()->depCache());
        Fix.ResolveByKeep();
    }

    if (!d->backend->areEventsCompressed()) {
        d->backend->packageChanged(this);
    }
}

void Package::setInstall()
{
    d->backend->cache()->depCache()->MarkInstall(*d->packageIter, true);

    // FIXME: can't we get rid of it here?
    // if there is something wrong, try to fix it
    if (!state() & ToInstall || d->backend->cache()->depCache()->BrokenCount() > 0) {
        pkgProblemResolver Fix(d->backend->cache()->depCache());
        Fix.Clear(*d->packageIter);
        Fix.Protect(*d->packageIter);
        Fix.Resolve(true);
    }

    if (!d->backend->areEventsCompressed()) {
        d->backend->packageChanged(this);
    }
}

void Package::setReInstall()
{
    d->backend->cache()->depCache()->SetReInstall(*d->packageIter, true);

    if (!d->backend->areEventsCompressed()) {
        d->backend->packageChanged(this);
    }
}


void Package::setRemove()
{
    pkgProblemResolver Fix(d->backend->cache()->depCache());

    Fix.Clear(*d->packageIter);
    Fix.Protect(*d->packageIter);
    Fix.Remove(*d->packageIter);

    Fix.InstallProtect();
    Fix.Resolve(true);

    d->backend->cache()->depCache()->SetReInstall(*d->packageIter, false);
    d->backend->cache()->depCache()->MarkDelete(*d->packageIter, false);

    if (!d->backend->areEventsCompressed()) {
        d->backend->packageChanged(this);
    }
}

void Package::setPurge()
{
    pkgProblemResolver Fix(d->backend->cache()->depCache());

    Fix.Clear(*d->packageIter);
    Fix.Protect(*d->packageIter);
    Fix.Remove(*d->packageIter);

    Fix.InstallProtect();
    Fix.Resolve(true);

    d->backend->cache()->depCache()->SetReInstall(*d->packageIter, false);
    d->backend->cache()->depCache()->MarkDelete(*d->packageIter, true);

    if (!d->backend->areEventsCompressed()) {
        d->backend->packageChanged(this);
    }
}

bool Package::setVersion(const QString &version)
{
    QLatin1String defaultCandVer("");
    pkgDepCache::StateCache &state = (*d->backend->cache()->depCache())[*d->packageIter];
    if (state.CandVersion != nullptr) {
        defaultCandVer = QLatin1String(state.CandVersion);
    }

    bool isDefault = (version == defaultCandVer);
    pkgVersionMatch Match(version.toLatin1().constData(), pkgVersionMatch::Version);
    const pkgCache::VerIterator &Ver = Match.Find(*d->packageIter);

    if (Ver.end()) {
        return false;
    }

    d->backend->cache()->depCache()->SetCandidateVersion(Ver);

    string archive;
    for (pkgCache::VerFileIterator VF = Ver.FileList();
         VF.end() == false;
         ++VF)
    {
        if (!VF.File() || !VF.File().Archive())
            continue;

        archive = VF.File().Archive();
        d->backend->cache()->depCache()->SetCandidateRelease(Ver, archive);
        break;
    }

    if (isDefault) {
        d->state &= ~OverrideVersion;
    } else {
        d->state |= OverrideVersion;
    }

    return true;
}

void Package::setPinned(bool pin)
{
    pin ? d->state |= IsPinned : d->state &= ~IsPinned;
}

}
