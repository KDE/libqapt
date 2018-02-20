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

//krazy:excludeall=qclasses
// Qt-only library, so things like QUrl *should* be used

#include "package.h"

// Qt includes
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QStringBuilder>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>

// Apt includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/acquire-item.h>
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
#include <random>

// Own includes
#include "backend.h"
#include "cache.h"
#include "config.h" // krazy:exclude=includes
#include "markingerrorinfo.h"

namespace QApt {

class PackagePrivate
{
    public:
        PackagePrivate(pkgCache::PkgIterator iter, Backend *back)
            : packageIter(iter)
            , backend(back)
            , state(0)
            , staticStateCalculated(false)
            , foreignArchCalculated(false)
            , isInUpdatePhase(false)
            , inUpdatePhaseCalculated(false)
        {
        }

        ~PackagePrivate()
        {
        }

        pkgCache::PkgIterator packageIter;
        QApt::Backend *backend;
        int state;
        bool staticStateCalculated;
        bool isForeignArch;
        bool foreignArchCalculated;
        bool isInUpdatePhase;
        bool inUpdatePhaseCalculated;

        pkgCache::PkgFileIterator searchPkgFileIter(QLatin1String label, const QString &release) const;
        QString getReleaseFileForOrigin(QLatin1String label, const QString &release) const;

        // Calculate state flags that cannot change
        void initStaticState(const pkgCache::VerIterator &ver, pkgDepCache::StateCache &stateCache);

        bool setInUpdatePhase(bool inUpdatePhase);
};

pkgCache::PkgFileIterator PackagePrivate::searchPkgFileIter(QLatin1String label, const QString &release) const
{
    pkgCache::VerIterator verIter = packageIter.VersionList();
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
    found = pkgCache::PkgFileIterator(*packageIter.Cache());
    return found;
}

QString PackagePrivate::getReleaseFileForOrigin(QLatin1String label, const QString &release) const
{
    pkgCache::PkgFileIterator pkg = searchPkgFileIter(label, release);

    // Return empty if no package matches the given label and release
    if (pkg.end())
        return QString();

    // Search for the matching meta-index
    pkgSourceList *list = backend->packageSourceList();
    pkgIndexFile *index;

    // Return empty if the source list doesn't contain an index for the package
    if (!list->FindIndex(pkg, index))
        return QString();

    for (auto I = list->begin(); I != list->end(); ++I) {
        vector<pkgIndexFile *> *ifv = (*I)->GetIndexFiles();
        if (find(ifv->begin(), ifv->end(), index) == ifv->end())
            continue;

        // Construct release file path
        QString uri = backend->config()->findDirectory("Dir::State::lists")
                % QString::fromStdString(URItoFileName((*I)->GetURI()))
                % QLatin1String("dists_")
                % QString::fromStdString((*I)->GetDist())
                % QLatin1String("_Release");

        return uri;
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
        }
    } else {
        packageState |= QApt::Package::NotInstalled;
    }

    // Broken/garbage statuses are constant until a cache reload
    if (stateCache.NowBroken()) {
        packageState |= QApt::Package::NowBroken;
    }

    if (stateCache.InstBroken()) {
        packageState |= QApt::Package::InstallBroken;
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

    // Essential/important status can only be changed by cache reload
    if (packageIter->Flags & (pkgCache::Flag::Important |
                             pkgCache::Flag::Essential)) {
        packageState |= QApt::Package::IsImportant;
    }

    if (packageIter->CurrentState == pkgCache::State::ConfigFiles) {
        packageState |= QApt::Package::ResidualConfig;
    }

    // Packages will stay undownloadable until a sources file is refreshed
    // and the cache is reloaded.
    bool downloadable = true;
    if (!stateCache.CandidateVer ||
        !stateCache.CandidateVerIter(*backend->cache()->depCache()).Downloadable())
        downloadable = false;

    if (!downloadable)
        packageState |= QApt::Package::NotDownloadable;

    state |= packageState;

    staticStateCalculated = true;
}

bool PackagePrivate::setInUpdatePhase(bool inUpdatePhase)
{
    inUpdatePhaseCalculated = true;
    isInUpdatePhase = inUpdatePhase;
    return inUpdatePhase;
}

Package::Package(QApt::Backend* backend, pkgCache::PkgIterator &packageIter)
        : d(new PackagePrivate(packageIter, backend))
{
}

Package::~Package()
{
    delete d;
}

const pkgCache::PkgIterator &Package::packageIterator() const
{
    return d->packageIter;
}

QLatin1String Package::name() const
{
    return QLatin1String(d->packageIter.Name());
}

int Package::id() const
{
    return d->packageIter->ID;
}

QLatin1String Package::section() const
{
    return QLatin1String(d->packageIter.Section());
}

QString Package::sourcePackage() const
{
    QString sourcePackage;

    // In the APT package record format, the only time when a "Source:" field
    // is present is when the binary package name doesn't match the source
    // name
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
    if (!ver.end()) {
        pkgRecords::Parser &rec = d->backend->records()->Lookup(ver.FileList());
        sourcePackage = QString::fromStdString(rec.SourcePkg());
    }

    // If the package record didn't have a "Source:" field, then this package's
    // name must be the source package's name. (Or there isn't a record for this package)
    if (sourcePackage.isEmpty()) {
        sourcePackage = name();
    }

    return sourcePackage;
}

QString Package::shortDescription() const
{
    QString shortDescription;
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
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
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

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
            if (sections[i].startsWith(QChar::Space)) {
                sections[i].remove(0, 1);
            }
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
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
    if (!ver.end()) {
        pkgRecords::Parser &parser = d->backend->records()->Lookup(ver.FileList());
        maintainer = QString::fromUtf8(parser.Maintainer().data());
        // This replacement prevents frontends from interpreting '<' as
        // an HTML tag opening
        maintainer.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    }
    return maintainer;
}

QString Package::homepage() const
{
    QString homepage;
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
    if (!ver.end()) {
        pkgRecords::Parser &parser = d->backend->records()->Lookup(ver.FileList());
        homepage = QString::fromUtf8(parser.Homepage().data());
    }
    return homepage;
}

QString Package::version() const
{
    if (!d->packageIter->CurrentVer) {
        pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[d->packageIter];
        if (!State.CandidateVer) {
            return QString();
        } else {
            return QLatin1String(State.CandidateVerIter(*d->backend->cache()->depCache()).VerStr());
        }
    } else {
        return QLatin1String(d->packageIter.CurrentVer().VerStr());
    }
}

QString Package::upstreamVersion() const
{
    const char *ver;

    if (!d->packageIter->CurrentVer) {
        pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[d->packageIter];
        if (!State.CandidateVer) {
            return QString();
        } else {
            ver = State.CandidateVerIter(*d->backend->cache()->depCache()).VerStr();
        }
    } else {
        ver = d->packageIter.CurrentVer().VerStr();
    }

    return QString::fromStdString(_system->VS->UpstreamVersion(ver));
}

QString Package::upstreamVersion(const QString &version)
{
    QByteArray ver = version.toLatin1();
    return QString::fromStdString(_system->VS->UpstreamVersion(ver.constData()));
}

QString Package::architecture() const
{
    pkgDepCache *depCache = d->backend->cache()->depCache();
    pkgCache::VerIterator ver = (*depCache)[d->packageIter].InstVerIter(*depCache);

    // the arch:all property is part of the version
    if (ver && ver.Arch())
        return QLatin1String(ver.Arch());

    return QLatin1String(d->packageIter.Arch());
}

QStringList Package::availableVersions() const
{
    QStringList versions;

    // Get available Versions.
    for (auto Ver = d->packageIter.VersionList(); !Ver.end(); ++Ver) {

        // We always take the first available version.
        pkgCache::VerFileIterator VF = Ver.FileList();
        if (VF.end())
            continue;

        pkgCache::PkgFileIterator File = VF.File();

        // Files without an archive will have a site
        QString archive = File.Archive()
                ? QLatin1String(File.Archive())
                : QLatin1String(File.Site());
        versions.append(QLatin1String(Ver.VerStr()) % QLatin1String(" (") %
                        archive % ')');
    }

    return versions;
}

QString Package::installedVersion() const
{
    if (!d->packageIter->CurrentVer) {
        return QString();
    }

    return QLatin1String(d->packageIter.CurrentVer().VerStr());
}

QString Package::availableVersion() const
{
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[d->packageIter];
    if (!State.CandidateVer) {
        return QString();
    }

    return QLatin1String(State.CandidateVerIter(*d->backend->cache()->depCache()).VerStr());
}

QString Package::priority() const
{
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
    if (ver.end())
        return QString();

    return QLatin1String(ver.PriorityType());
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
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

    if(Ver.end())
        return QString();

    pkgCache::VerFileIterator VF = Ver.FileList();
    return QString::fromUtf8(VF.File().Origin());
}

QString Package::site() const
{
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

    if(Ver.end())
        return QString();

    pkgCache::VerFileIterator VF = Ver.FileList();
    return QString::fromUtf8(VF.File().Site());
}

QStringList Package::archives() const
{
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

    if(Ver.end())
        return QStringList();

    QStringList archiveList;
    for (auto VF = Ver.FileList(); !VF.end(); ++VF)
        archiveList << QLatin1String(VF.File().Archive());

    return archiveList;
}

QString Package::component() const
{
    QString sect = section();
    if(sect.isEmpty())
        return QString();

    QStringList split = sect.split('/');

    if (split.count() > 1)
        return split.first();

    return QString("main");
}

QByteArray Package::md5Sum() const
{
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

    if(ver.end())
        return QByteArray();

    pkgRecords::Parser &rec = d->backend->records()->Lookup(ver.FileList());

    return rec.MD5Hash().c_str();

}

QUrl Package::changelogUrl() const
{
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
    if (ver.end())
        return QUrl();

    const QString url = QString::fromStdString(pkgAcqChangelog::URI(ver));

    // pkgAcqChangelog::URI(ver) may return URIs with schemes other than http(s)
    // e.g. copy:// gzip:// for local files. We exclude them for backward
    // compatibility with libQApt <= 3.0.3.
    if (!url.startsWith("http"))
        return QUrl();

    return QUrl(url);
}

QUrl Package::screenshotUrl(QApt::ScreenshotType type) const
{
    QUrl url;
    switch (type) {
        case QApt::Thumbnail:
            url = QUrl(controlField(QLatin1String("Thumbnail-Url")));
            if(url.isEmpty())
                url = QUrl("http://screenshots.debian.net/thumbnail/" % name());
            break;
        case QApt::Screenshot:
            url = QUrl(controlField(QLatin1String("Screenshot-Url")));
            if(url.isEmpty())
                url = QUrl("http://screenshots.debian.net/screenshot/" % name());
            break;
        default:
            qDebug() << "I do not know how to handle the screenshot type given to me: " << QString::number(type);
    }

    return url;
}

QDateTime Package::supportedUntil() const
{
    if (!isSupported()) {
        return QDateTime();
    }

    QFile lsb_release(QLatin1String("/etc/lsb-release"));
    if (!lsb_release.open(QFile::ReadOnly)) {
        // Though really, your system is screwed if this happens...
        return QDateTime();
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
        return QDateTime();
    }

    // read the relase file
    FileFd fd(releaseFile.toStdString(), FileFd::ReadOnly);
    pkgTagFile tag(&fd);
    tag.Step(sec);

    if(!RFC1123StrToTime(sec.FindS("Date").data(), releaseDate)) {
        return QDateTime();
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

    return supportEnd;
}

QString Package::controlField(QLatin1String name) const
{
    const pkgCache::VerIterator &ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
    if (ver.end()) {
        return QString();
    }

    pkgRecords::Parser &rec = d->backend->records()->Lookup(ver.FileList());

    return QString::fromStdString(rec.RecordField(name.latin1()));
}

QString Package::controlField(const QString &name) const
{
    return controlField(QLatin1String(name.toLatin1()));
}

qint64 Package::currentInstalledSize() const
{
    const pkgCache::VerIterator &ver = d->packageIter.CurrentVer();

    if (!ver.end()) {
        return qint64(ver->InstalledSize);
    } else {
        return qint64(-1);
    }
}

qint64 Package::availableInstalledSize() const
{
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[d->packageIter];
    if (!State.CandidateVer) {
        return qint64(-1);
    }
    return qint64(State.CandidateVerIter(*d->backend->cache()->depCache())->InstalledSize);
}

qint64 Package::downloadSize() const
{
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[d->packageIter];
    if (!State.CandidateVer) {
        return qint64(-1);
    }

    return qint64(State.CandidateVerIter(*d->backend->cache()->depCache())->Size);
}

int Package::state() const
{
    int packageState = 0;

    const pkgCache::VerIterator &ver = d->packageIter.CurrentVer();
    pkgDepCache::StateCache &stateCache = (*d->backend->cache()->depCache())[d->packageIter];

    if (!d->staticStateCalculated) {
        d->initStaticState(ver, stateCache);
    }

    if (stateCache.Install()) {
        packageState |= ToInstall;
    }

    if (stateCache.Flags & pkgCache::Flag::Auto) {
        packageState |= QApt::Package::IsAuto;
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
        if (stateCache.Held()) {
            packageState |= QApt::Package::Held;
        }
    }

   return packageState | d->state;
}

int Package::staticState() const
{
    if (!d->staticStateCalculated) {
        const pkgCache::VerIterator &ver = d->packageIter.CurrentVer();
        pkgDepCache::StateCache &stateCache = (*d->backend->cache()->depCache())[d->packageIter];
        d->initStaticState(ver, stateCache);
    }

    return d->state;
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
    return !d->packageIter.CurrentVer().end();
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

bool Package::isInUpdatePhase() const
{
    if (!(state() & Package::Upgradeable)) {
        return false;
    }

    // Try to use the cached values, otherwise we have to do the calculation.
    if (d->inUpdatePhaseCalculated) {
        return d->isInUpdatePhase;
    }

    bool intConversionOk = true;
    int phasedUpdatePercent = controlField(QLatin1String("Phased-Update-Percentage")).toInt(&intConversionOk);
    if (!intConversionOk) {
        // Upgradable but either the phase percent field is not there at all
        // or it has a non-convertable value.
        // In either case this package is good for upgrade.
        return d->setInUpdatePhase(true);
    }

    // This is a more or less an exact reimplemenation of the update phasing
    // algorithm Ubuntu uses.
    // Deciding whether a machine is in the phasing pool or not happens in
    // two steps.
    // 1. repeatable random number generation between 0..100
    // 2. comparision of random number with phasing percentage and marking
    //    as upgradable if rand is greater than the phasing.

    // Repeatable discrete random number generation is based on
    // the MD5 hash of "sourcename-sourceversion-dbusmachineid", this
    // hash is used as seed for the random number generator to provide
    // stable randomness based on the stable seed. Combined with the discrete
    // quasi-randomiziation we get about even distribution of machines accross
    // phases.
    static QString machineId;
    if (machineId.isNull()) {
        QFile file(QStringLiteral("/var/lib/dbus/machine-id"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            machineId = file.readLine().trimmed();
        }
    }

    if (machineId.isEmpty()) {
        // Without machineId we cannot differentiate one machine from another, so
        // we have no way to build a unique hash.
        return true; // Don't change cache as we might have more luck next time.
    }

    QString seedString = QStringLiteral("%1-%2-%3").arg(sourcePackage(),
                                                        availableVersion(),
                                                        machineId);
    QByteArray seed = QCryptographicHash::hash(seedString.toLatin1(), QCryptographicHash::Md5);
    // MD5 would be 128bits, that's two quint64 stdlib random default_engine
    // uses a uint32 seed though, so we'd loose precision anyway, so screw
    // this, we'll get the first 32bit and screw the rest! This is not
    // rocket science, worst case the update only arrives once the phasing
    // tag is removed.
    seed = seed.toHex();
    seed.truncate(8 /* each character in a hex string values 4 bit, 8*4=32bit */);

    bool ok = false;
    uint a = seed.toUInt(&ok, 16);
    Q_ASSERT(ok); // Hex conversion always is supposed to work at this point.

    std::default_random_engine generator(a);
    std::uniform_int_distribution<int> distribution(0, 100);
    int rand = distribution(generator);

    // rand is the percentage at which the machine starts to be in the phase.
    // Once rand is less than the phasing percentage e.g. 40rand vs. 50phase
    // the machine is supposed to start phasing.
    return d->setInUpdatePhase(rand <= phasedUpdatePercent);
}

bool Package::isMultiArchDuplicate() const
{
    // Excludes installed packages, which are always "interesting"
    if (isInstalled())
        return false;

    // Otherwise, check if the pkgIterator is the "best" from its group
    return (d->packageIter.Group().FindPkg() != d->packageIter);
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
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[d->packageIter];

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

    for(pkgCache::DepIterator it = d->packageIter.RevDependsList(); !it.end(); ++it) {
        reverseDependsList << QLatin1String(it.ParentPkg().Name());
    }

    return reverseDependsList;
}

QStringList Package::providesList() const
{
    pkgDepCache::StateCache &State = (*d->backend->cache()->depCache())[d->packageIter];
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

    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

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

    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

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

    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

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
        if (package->enhancesList().contains(name())) {
            enhancedByList << package->name();
        }
    }

    return enhancedByList;
}


QList<QApt::MarkingErrorInfo> Package::brokenReason() const
{
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);
    QList<MarkingErrorInfo> reasons;

    // check if there is actually something to install
    if (!Ver) {
        QApt::DependencyInfo info(name(), QString(), NoOperand, InvalidType);
        QApt::MarkingErrorInfo error(QApt::ParentNotInstallable, info);
        reasons.append(error);
        return reasons;
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
                QString targetName = QLatin1String(Start.TargetPkg().Name());
                QApt::DependencyType relation = (QApt::DependencyType)End->Type;

                QApt::DependencyInfo errorInfo(targetName, requiredVersion,
                                               NoOperand, relation);
                QApt::MarkingErrorInfo error(QApt::WrongCandidateVersion, errorInfo);
                reasons.append(error);
            } else { // We have the package, but for some reason it won't be installed
                // In this case, the required version does not exist at all
                QString targetName = QLatin1String(Start.TargetPkg().Name());
                QApt::DependencyType relation = (QApt::DependencyType)End->Type;

                QApt::DependencyInfo errorInfo(targetName, requiredVersion,
                                               NoOperand, relation);
                QApt::MarkingErrorInfo error(QApt::DepNotInstallable, errorInfo);
                reasons.append(error);
            }
        } else {
            // Ok, candidate has provides. We're a virtual package
            QString targetName = QLatin1String(Start.TargetPkg().Name());
            QApt::DependencyType relation = (QApt::DependencyType)End->Type;

            QApt::DependencyInfo errorInfo(targetName, QString(),
                                           NoOperand, relation);
            QApt::MarkingErrorInfo error(QApt::VirtualPackage, errorInfo);
            reasons.append(error);
        }
    }

    return reasons;
}

bool Package::isTrusted() const
{
    const pkgCache::VerIterator &Ver = (*d->backend->cache()->depCache()).GetCandidateVer(d->packageIter);

    if (!Ver)
        return false;

    pkgSourceList *Sources = d->backend->packageSourceList();
    QHash<pkgCache::PkgFileIterator, pkgIndexFile*> *trustCache = d->backend->cache()->trustCache();

    for (pkgCache::VerFileIterator i = Ver.FileList(); !i.end(); ++i)
    {
        pkgIndexFile *Index;

        //FIXME: Should be done in apt
        auto trustIter = trustCache->constBegin();
        while (trustIter != trustCache->constEnd()) {
            if (trustIter.key() == i.File())
                break; // Found it
            trustIter++;
        }

        // Find the index of the package file from the package sources
        if (trustIter == trustCache->constEnd()) { // Not found
            if (!Sources->FindIndex(i.File(), Index))
              continue;
        } else
            Index = trustIter.value();

        if (Index->IsTrusted())
            return true;
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
    d->backend->cache()->depCache()->MarkAuto(d->packageIter, flag);
}


void Package::setKeep()
{
    d->backend->cache()->depCache()->MarkKeep(d->packageIter, false);
    if (state() & ToReInstall) {
        d->backend->cache()->depCache()->SetReInstall(d->packageIter, false);
    }
    if (d->backend->cache()->depCache()->BrokenCount() > 0) {
        pkgProblemResolver Fix(d->backend->cache()->depCache());
        Fix.ResolveByKeep();
    }

    d->state |= IsManuallyHeld;

    if (!d->backend->areEventsCompressed()) {
        d->backend->emitPackageChanged();
    }
}

void Package::setInstall()
{
    d->backend->cache()->depCache()->MarkInstall(d->packageIter, true);
    d->state &= ~IsManuallyHeld;

    // FIXME: can't we get rid of it here?
    // if there is something wrong, try to fix it
    if (!state() & ToInstall || d->backend->cache()->depCache()->BrokenCount() > 0) {
        pkgProblemResolver Fix(d->backend->cache()->depCache());
        Fix.Clear(d->packageIter);
        Fix.Protect(d->packageIter);
        Fix.Resolve(true);
    }

    if (!d->backend->areEventsCompressed()) {
        d->backend->emitPackageChanged();
    }
}

void Package::setReInstall()
{
    d->backend->cache()->depCache()->SetReInstall(d->packageIter, true);
    d->state &= ~IsManuallyHeld;

    if (!d->backend->areEventsCompressed()) {
        d->backend->emitPackageChanged();
    }
}

// TODO: merge into one function with bool_purge param
void Package::setRemove()
{
    pkgProblemResolver Fix(d->backend->cache()->depCache());

    Fix.Clear(d->packageIter);
    Fix.Protect(d->packageIter);
    Fix.Remove(d->packageIter);

    d->backend->cache()->depCache()->SetReInstall(d->packageIter, false);
    d->backend->cache()->depCache()->MarkDelete(d->packageIter, false);

    Fix.Resolve(true);

    d->state &= ~IsManuallyHeld;

    if (!d->backend->areEventsCompressed()) {
        d->backend->emitPackageChanged();
    }
}

void Package::setPurge()
{
    pkgProblemResolver Fix(d->backend->cache()->depCache());

    Fix.Clear(d->packageIter);
    Fix.Protect(d->packageIter);
    Fix.Remove(d->packageIter);

    d->backend->cache()->depCache()->SetReInstall(d->packageIter, false);
    d->backend->cache()->depCache()->MarkDelete(d->packageIter, true);

    Fix.Resolve(true);

    d->state &= ~IsManuallyHeld;

    if (!d->backend->areEventsCompressed()) {
        d->backend->emitPackageChanged();
    }
}

bool Package::setVersion(const QString &version)
{
    pkgDepCache::StateCache &state = (*d->backend->cache()->depCache())[d->packageIter];
    QLatin1String defaultCandVer(state.CandVersion);

    bool isDefault = (version == defaultCandVer);
    pkgVersionMatch Match(version.toLatin1().constData(), pkgVersionMatch::Version);
    const pkgCache::VerIterator &Ver = Match.Find(d->packageIter);

    if (Ver.end())
        return false;

    d->backend->cache()->depCache()->SetCandidateVersion(Ver);

    for (auto VF = Ver.FileList(); !VF.end(); ++VF) {
        if (!VF.File() || !VF.File().Archive())
            continue;

        d->backend->cache()->depCache()->SetCandidateRelease(Ver, VF.File().Archive());
        break;
    }

    if (isDefault)
        d->state &= ~OverrideVersion;
    else
        d->state |= OverrideVersion;

    return true;
}

void Package::setPinned(bool pin)
{
    pin ? d->state |= IsPinned : d->state &= ~IsPinned;
}

}
