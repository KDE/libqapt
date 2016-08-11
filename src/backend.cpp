/***************************************************************************
 *   Copyright © 2010-2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
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

#include "backend.h"

// Qt includes
#include <QtCore/QByteArray>
#include <QtCore/QTemporaryFile>
#include <QtDBus/QDBusConnection>

// Apt includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/init.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>

// Xapian includes
#undef slots
#include <xapian.h>

// QApt includes
#include "cache.h"
#include "config.h" // krazy:exclude=includes
#include "dbusinterfaces_p.h"
#include "debfile.h"
#include "transaction.h"

namespace QApt {

class BackendPrivate
{
public:
    BackendPrivate()
        : cache(nullptr)
        , records(nullptr)
        , maxStackSize(20)
        , xapianDatabase(nullptr)
        , xapianIndexExists(false)
        , config(nullptr)
        , actionGroup(nullptr)
        , frontendCaps(QApt::NoCaps)
    {
    }
    ~BackendPrivate()
    {
        qDeleteAll(packages);
        delete cache;
        delete records;
        delete config;
        delete xapianDatabase;
        delete actionGroup;
    }
    // Caches
    // The canonical list of all unique, non-virutal package objects
    PackageList packages;
    // A list of each package object's ID number
    QVector<int> packagesIndex;
    // Set of group names extracted from our packages
    QSet<Group> groups;
    // Cache of origin/human-readable name pairings
    QHash<QString, QString> originMap;
    // Relation of an origin and its hostname
    QHash<QString, QString> siteMap;

    // Counts
    int installedCount;

    // Pointer to the apt cache object
    Cache *cache;
    pkgRecords *records;

    // Undo/redo stuff
    int maxStackSize;
    QList<CacheState> undoStack;
    QList<CacheState> redoStack;

    // Xapian
    time_t xapianTimeStamp;
    Xapian::Database *xapianDatabase;
    bool xapianIndexExists;

    // DBus
    WorkerInterface *worker;

    // Config
    Config *config;
    bool isMultiArch;
    QString nativeArch;

    // Event compression
    bool compressEvents;
    pkgDepCache::ActionGroup *actionGroup;

    // Other
    bool writeSelectionFile(const QString &file, const QString &path) const;
    QString customProxy;
    QString initErrorMessage;
    QApt::FrontendCaps frontendCaps;
};

bool BackendPrivate::writeSelectionFile(const QString &selectionDocument, const QString &path) const
{
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        return false;
    } else {
        QTextStream out(&file);
        out << selectionDocument;
    }

    return true;
}

Backend::Backend(QObject *parent)
        : QObject(parent)
        , d_ptr(new BackendPrivate)
{
    Q_D(Backend);

    d->worker = new WorkerInterface(QLatin1String(s_workerReverseDomainName),
                                    QLatin1String("/"), QDBusConnection::systemBus(),
                                    this);
    connect(d->worker, SIGNAL(transactionQueueChanged(QString,QStringList)),
            this, SIGNAL(transactionQueueChanged(QString,QStringList)));
    DownloadProgress::registerMetaTypes();
}

Backend::~Backend()
{
    delete d_ptr;
}

bool Backend::init()
{
    Q_D(Backend);
    if (!pkgInitConfig(*_config) || !pkgInitSystem(*_config, _system)) {
        setInitError();
        return false;
    }

    d->cache = new Cache(this);
    d->config = new Config(this);
    d->nativeArch = config()->readEntry(QLatin1String("APT::Architecture"),
                                        QLatin1String(""));
    openXapianIndex();

    return reloadCache();
}

bool Backend::reloadCache()
{
    Q_D(Backend);

    emit cacheReloadStarted();

    if (!d->cache->open()) {
        setInitError();
        return false;
    }

    pkgDepCache *depCache = d->cache->depCache();

    delete d->records;
    d->records = new pkgRecords(*depCache);

    qDeleteAll(d->packages);
    d->packages.clear();
    d->groups.clear();
    d->originMap.clear();
    d->siteMap.clear();
    d->packagesIndex.clear();
    d->installedCount = 0;

    int packageCount = depCache->Head().PackageCount;
    d->packagesIndex.resize(packageCount);
    d->packagesIndex.fill(-1);
    d->packages.reserve(packageCount);

    // Populate internal package cache
    int count = 0;

    d->isMultiArch = architectures().size() > 1;

    pkgCache::PkgIterator iter;
    for (iter = depCache->PkgBegin(); !iter.end(); ++iter) {
        if (!iter->VersionList) {
            continue; // Exclude virtual packages.
        }

        Package *pkg = new Package(this, iter);

        d->packagesIndex[iter->ID] = count;
        d->packages.append(pkg);
        ++count;

        if (iter->CurrentVer) {
            d->installedCount++;
        }

        QString group = pkg->section();

        // Populate groups
        if (!group.isEmpty()) {
            d->groups << group;
        }

        pkgCache::VerIterator Ver = (*depCache)[iter].CandidateVerIter(*depCache);

        if(!Ver.end()) {
            const pkgCache::VerFileIterator VF = Ver.FileList();
            const QString origin(QLatin1String(VF.File().Origin()));
            d->originMap[origin] = QLatin1String(VF.File().Label());
            d->siteMap[origin] = QLatin1String(VF.File().Site());
        }
    }

    d->originMap.remove(QString());

    d->undoStack.clear();
    d->redoStack.clear();

    // Determine which packages are pinned for display purposes
    loadPackagePins();

    emit cacheReloadFinished();

    return true;
}

void Backend::setInitError()
{
    Q_D(Backend);

    string message;
    if (_error->PopMessage(message))
        d->initErrorMessage = QString::fromStdString(message);
}

void Backend::loadPackagePins()
{
    Q_D(Backend);

    QString dirBase = d->config->findDirectory(QLatin1String("Dir::Etc"));
    QString dir = dirBase % QLatin1String("preferences.d/");
    QDir logDirectory(dir);
    QStringList pinFiles = logDirectory.entryList(QDir::Files, QDir::Name);
    pinFiles << dirBase % QLatin1String("preferences");

    for (const QString &pinName : pinFiles) {
        // Make all paths absolute
        QString pinPath = pinName.startsWith('/') ? pinName : dir % pinName;

        if (!QFile::exists(pinPath))
                continue;

        FileFd Fd(pinPath.toUtf8().data(), FileFd::ReadOnly);

        pkgTagFile tagFile(&Fd);
        if (_error->PendingError()) {
            _error->Discard();
            continue;
        }

        pkgTagSection tags;
        while (tagFile.Step(tags)) {
            string name = tags.FindS("Package");
            Package *pkg = package(QLatin1String(name.c_str()));
            if (pkg)
                pkg->setPinned(true);
        }
    }
}

QString Backend::initErrorMessage() const
{
    Q_D(const Backend);

    return d->initErrorMessage;
}

pkgSourceList *Backend::packageSourceList() const
{
    Q_D(const Backend);

    return d->cache->list();
}

Cache *Backend::cache() const
{
    Q_D(const Backend);

    return d->cache;
}

pkgRecords *Backend::records() const
{
    Q_D(const Backend);

    return d->records;
}

Package *Backend::package(pkgCache::PkgIterator &iter) const
{
    Q_D(const Backend);

    int index = d->packagesIndex.at(iter->ID);
    if (index != -1 && index < d->packages.size()) {
        return d->packages.at(index);
    }

    return nullptr;
}

Package *Backend::package(const QString &name) const
{
    return package(QLatin1String(name.toLatin1()));
}

Package *Backend::package(QLatin1String name) const
{
    Q_D(const Backend);

    pkgCache::PkgIterator pkg = d->cache->depCache()->FindPkg(name.latin1());
    if (!pkg.end()) {
        return package(pkg);
    }
    return nullptr;
}

Package *Backend::packageForFile(const QString &file) const
{
    Q_D(const Backend);

    if (file.isEmpty()) {
        return nullptr;
    }

    for (Package *package : d->packages) {
        if (package->installedFilesList().contains(file)) {
            return package;
        }
    }

    return nullptr;
}

QStringList Backend::origins() const
{
    Q_D(const Backend);

    return d->originMap.keys();
}

QStringList Backend::originLabels() const
{
    Q_D(const Backend);

    return d->originMap.values();
}

QString Backend::originLabel(const QString &origin) const
{
    Q_D(const Backend);

    return d->originMap.value(origin);
}

QString Backend::origin(const QString &originLabel) const
{
    Q_D(const Backend);

    return d->originMap.key(originLabel);
}

QStringList Backend::originsForHost(const QString& host) const
{
    Q_D(const Backend);
    return d->siteMap.keys(host);
}

int Backend::packageCount() const
{
    Q_D(const Backend);

    return d->packages.size();
}

int Backend::packageCount(const Package::States &states) const
{
    Q_D(const Backend);

    int packageCount = 0;

    for (const Package *package : d->packages) {
        if ((package->state() & states)) {
            packageCount++;
        }
    }

    return packageCount;
}

int Backend::installedCount() const
{
    Q_D(const Backend);

    return d->installedCount;
}

int Backend::toInstallCount() const
{
    Q_D(const Backend);

    return d->cache->depCache()->InstCount();
}

int Backend::toRemoveCount() const
{
    Q_D(const Backend);

    return d->cache->depCache()->DelCount();
}

qint64 Backend::downloadSize() const
{
    Q_D(const Backend);

    // Raw size, ignoring already-downloaded or partially downloaded archives
    qint64 downloadSize = d->cache->depCache()->DebSize();

    // If downloadSize() is called during a cache refresh, there is a chance it
    // will do so at a bad time and produce an error. Discard any errors that
    // happen during this function, since they will always be innocuous and at
    // worst will result in the less accurate DebSize() number being returned
    _error->PushToStack();

    // If possible, get what really needs to be downloaded
    pkgAcquire fetcher;
    pkgPackageManager *PM = _system->CreatePM(d->cache->depCache());
    if (PM->GetArchives(&fetcher, d->cache->list(), d->records)) {
        downloadSize = fetcher.FetchNeeded();
    }
    delete PM;

    _error->Discard();
    _error->RevertToStack();

    return downloadSize;
}

qint64 Backend::installSize() const
{
    Q_D(const Backend);

    qint64 installSize = d->cache->depCache()->UsrSize();

    return installSize;
}

PackageList Backend::availablePackages() const
{
    Q_D(const Backend);

    return d->packages;
}

PackageList Backend::upgradeablePackages() const
{
    Q_D(const Backend);

    PackageList upgradeablePackages;

    for (Package *package : d->packages) {
        if (package->staticState() & Package::Upgradeable) {
            upgradeablePackages << package;
        }
    }

    return upgradeablePackages;
}

PackageList Backend::markedPackages() const
{
    Q_D(const Backend);

    PackageList markedPackages;

    for (Package *package : d->packages) {
        if (package->state() & (Package::ToInstall | Package::ToReInstall |
                                Package::ToUpgrade | Package::ToDowngrade |
                                Package::ToRemove | Package::ToPurge)) {
            markedPackages << package;
        }
    }
    return markedPackages;
}

PackageList Backend::search(const QString &searchString) const
{
    Q_D(const Backend);

    if (d->xapianTimeStamp == 0 || !d->xapianDatabase) {
        return QApt::PackageList();
    }

    string unsplitSearchString = searchString.toStdString();
    static int qualityCutoff = 15;
    PackageList searchResult;

    // Doesn't follow style guidelines to ease merging with synaptic
    try {
        int maxItems = d->xapianDatabase->get_doccount();
        Xapian::Enquire enquire(*(d->xapianDatabase));
        Xapian::QueryParser parser;
        parser.set_database(*(d->xapianDatabase));
        parser.add_prefix("name","XP");
        parser.add_prefix("section","XS");
        // default op is AND to narrow down the resultset
        parser.set_default_op( Xapian::Query::OP_AND );

        /* Workaround to allow searching an hyphenated package name using a prefix (name:)
        * LP: #282995
        * Xapian currently doesn't support wildcard for boolean prefix and
        * doesn't handle implicit wildcards at the end of hypenated phrases.
        *
        * e.g searching for name:ubuntu-res will be equivalent to 'name:ubuntu res*'
        * however 'name:(ubuntu* res*) won't return any result because the
        * index is built with the full package name
        */
        // Always search for the package name
        string xpString = "name:";
        string::size_type pos = unsplitSearchString.find_first_of(" ,;");
        if (pos > 0) {
            xpString += unsplitSearchString.substr(0,pos);
        } else {
            xpString += unsplitSearchString;
        }
        Xapian::Query xpQuery = parser.parse_query(xpString);

        pos = 0;
        while ( (pos = unsplitSearchString.find("-", pos)) != string::npos ) {
            unsplitSearchString.replace(pos, 1, " ");
            pos+=1;
        }

        // Build the query
        // apply a weight factor to XP term to increase relevancy on package name
        Xapian::Query query = parser.parse_query(unsplitSearchString,
           Xapian::QueryParser::FLAG_WILDCARD |
           Xapian::QueryParser::FLAG_BOOLEAN |
           Xapian::QueryParser::FLAG_PARTIAL);
        query = Xapian::Query(Xapian::Query::OP_OR, query,
                Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, xpQuery, 3));
        enquire.set_query(query);
        Xapian::MSet matches = enquire.get_mset(0, maxItems);

      // Retrieve the results
      int top_percent = 0;
      for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i)
      {
         std::string pkgName = i.get_document().get_data();
         Package* pkg = package(QLatin1String(pkgName.c_str()));
         // Filter out results that apt doesn't know
         if (!pkg)
            continue;

         // Save the confidence interval of the top value, to use it as
         // a reference to compute an adaptive quality cutoff
         if (top_percent == 0)
            top_percent = i.get_percent();

         // Stop producing if the quality goes below a cutoff point
         if (i.get_percent() < qualityCutoff * top_percent / 100)
         {
            break;
         }

         searchResult.append(pkg);
         }
    } catch (const Xapian::Error & error) {
        qDebug() << "Search error" << QString::fromStdString(error.get_msg());
        return QApt::PackageList();
    }

    return searchResult;
}

GroupList Backend::availableGroups() const
{
    Q_D(const Backend);

    GroupList groupList = d->groups.toList();

    return groupList;
}

bool Backend::isMultiArchEnabled() const
{
    Q_D(const Backend);

    return d->isMultiArch;
}

QStringList Backend::architectures() const
{
    Q_D(const Backend);

    return d->config->architectures();
}

QString Backend::nativeArchitecture() const
{
    Q_D(const Backend);

    return d->nativeArch;
}

bool Backend::areChangesMarked() const
{
    return (toInstallCount() + toRemoveCount());
}

bool Backend::isBroken() const
{
    Q_D(const Backend);

    if (!d->cache->depCache()) {
        return true;
    }

    // Check for broken things
    if (d->cache->depCache()->BrokenCount()) {
        return true;
    }

    return false;
}

QApt::FrontendCaps Backend::frontendCaps() const
{
    Q_D(const Backend);

    return d->frontendCaps;
}

QDateTime Backend::timeCacheLastUpdated() const
{
    QDateTime sinceUpdate;

    QFileInfo updateStamp("/var/lib/apt/periodic/update-success-stamp");
    if (!updateStamp.exists())
        return sinceUpdate;

    return updateStamp.lastModified();
}

bool Backend::xapianIndexNeedsUpdate() const
{
    Q_D(const Backend);

    // If the cache has been modified after the xapian timestamp, we need to rebuild
    QDateTime aptCacheTime = QFileInfo(d->config->findFile("Dir::Cache::pkgcache")).lastModified();
    QDateTime dpkgStatusTime = QFileInfo(d->config->findFile("Dir::State::status")).lastModified();

    bool outdated = false;
    if (d->xapianTimeStamp < aptCacheTime.toTime_t() ||
            d->xapianTimeStamp < dpkgStatusTime.toTime_t()) {
        outdated = true;
    }

    return (outdated || (!d->xapianIndexExists));
}

bool Backend::openXapianIndex()
{
    Q_D(Backend);

    QFileInfo timeStamp(QLatin1String("/var/lib/apt-xapian-index/update-timestamp"));
    d->xapianTimeStamp = timeStamp.lastModified().toTime_t();

    if(d->xapianDatabase) {
        delete d->xapianDatabase;
        d->xapianDatabase = 0;
    }
    try {
        d->xapianDatabase = new Xapian::Database("/var/lib/apt-xapian-index/index");
        d->xapianIndexExists = true;
    } catch (Xapian::DatabaseOpeningError) {
        d->xapianIndexExists = false;
        return false;
    };

    return true;
}

Config *Backend::config() const
{
    Q_D(const Backend);

    return d->config;
}

CacheState Backend::currentCacheState() const
{
    Q_D(const Backend);

    CacheState state;
    int pkgSize = d->packages.size();
    state.reserve(pkgSize);
    for (int i = 0; i < pkgSize; ++i) {
        state.append(d->packages.at(i)->state());
    }

    return state;
}

QHash<Package::State, PackageList> Backend::stateChanges(const CacheState &oldState,
                                                         const PackageList &excluded) const
{
    Q_D(const Backend);

    QHash<Package::State, PackageList> changes;

    // Return an empty change set for invalid state caches
    if (oldState.isEmpty())
        return changes;

    Q_ASSERT(d->packages.size() == oldState.size());

    for (int i = 0; i < d->packages.size(); ++i) {
        Package *pkg = d->packages.at(i);

        if (excluded.contains(pkg))
            continue;

        int status = pkg->state();

        if (oldState.at(i) == status)
            continue;

        // These flags will never be set together.
        // We can use this to filter status down to a single flag.
        status &= (Package::Held |
                   Package::NewInstall |
                   Package::ToReInstall |
                   Package::ToUpgrade |
                   Package::ToDowngrade |
                   Package::ToRemove);

        if (status == 0) {
            qWarning() << "Package" << pkg->name() << "had a state change,"
                       << "it can however not be presented as a unique state."
                       << "This is often an indication that the package is"
                       << "supposed to be upgraded but can't because its"
                       << "dependencies are not satisfied. This is not"
                       << "considered a held package unless its upgrade is"
                       << "necessary or causing breakage. A simple unsatisfied"
                       << "dependency without the need to upgrade is not"
                       << "considered an issue and thus not reported.\n"
                       << "States were:"
                       << (Package::States)oldState.at(i)
                       << "->"
                       << (Package::States)pkg->state();
            // Apt pretends packages like this are not held (which is reflected)
            // in the state loss. Whether or not this is intentional is not
            // obvious at the time of writing in case it isn't the states
            // here would add up again and the package rightfully would be
            // reported as held. So we would never get here.
            // Until then ignore these packages as we cannot serialize their
            // state anyway.
            continue;
        }
        // Add this package/status pair to the changes hash
        PackageList list = changes.value((Package::State)status);
        list.append(pkg);
        changes[(Package::State)status]= list;
    }

    return changes;
}

void Backend::saveCacheState()
{
    Q_D(Backend);
    CacheState state = currentCacheState();
    d->undoStack.prepend(state);
    d->redoStack.clear();

    while (d->undoStack.size() > d->maxStackSize) {
        d->undoStack.removeLast();
    }
}

void Backend::restoreCacheState(const CacheState &state)
{
    Q_D(Backend);

    pkgDepCache *deps = d->cache->depCache();
    pkgDepCache::ActionGroup group(*deps);

    int packageCount = d->packages.size();
    for (int i = 0; i < packageCount; ++i) {
        Package *pkg = d->packages.at(i);
        int flags = pkg->state();
        int oldflags = state.at(i);

        if (oldflags == flags)
            continue;

        if ((flags & Package::ToReInstall) && !(oldflags & Package::ToReInstall)) {
            deps->SetReInstall(pkg->packageIterator(), false);
        }

        if (oldflags & Package::ToReInstall) {
            deps->MarkInstall(pkg->packageIterator(), true);
            deps->SetReInstall(pkg->packageIterator(), true);
        } else if (oldflags & Package::ToInstall) {
            deps->MarkInstall(pkg->packageIterator(), true);
        } else if (oldflags & Package::ToRemove) {
            deps->MarkDelete(pkg->packageIterator(), (bool)(oldflags & Package::ToPurge));
        } else if (oldflags & Package::ToKeep) {
            deps->MarkKeep(pkg->packageIterator(), false);
        }
        // fix the auto flag
        deps->MarkAuto(pkg->packageIterator(), (oldflags & Package::IsAuto));
    }

    emit packageChanged();
}

void Backend::setUndoRedoCacheSize(int newSize)
{
    Q_D(Backend);

    d->maxStackSize = newSize;
}

bool Backend::isUndoStackEmpty() const
{
    Q_D(const Backend);

    return d->undoStack.isEmpty();
}

bool Backend::isRedoStackEmpty() const
{
    Q_D(const Backend);

    return d->redoStack.isEmpty();
}

bool Backend::areEventsCompressed() const
{
    Q_D(const Backend);

    return d->actionGroup != nullptr;
}

void Backend::undo()
{
    Q_D(Backend);

    if (d->undoStack.isEmpty()) {
        return;
    }

    // Place current state on redo stack
    d->redoStack.prepend(currentCacheState());

    CacheState state = d->undoStack.takeFirst();
    restoreCacheState(state);
}

void Backend::redo()
{
    Q_D(Backend);

    if (d->redoStack.isEmpty()) {
        return;
    }

    // Place current state on undo stack
    d->undoStack.append(currentCacheState());

    CacheState state = d->redoStack.takeFirst();
    restoreCacheState(state);
}

void Backend::markPackagesForUpgrade()
{
    Q_D(Backend);

    pkgAllUpgrade(*d->cache->depCache());
    emit packageChanged();
}

void Backend::markPackagesForDistUpgrade()
{
    Q_D(Backend);

    pkgDistUpgrade(*d->cache->depCache());
    emit packageChanged();
}

void Backend::markPackagesForAutoRemove()
{
    Q_D(Backend);

    pkgDepCache &cache = *d->cache->depCache();
    bool isResidual;

    for (pkgCache::PkgIterator pkgIter = cache.PkgBegin(); !pkgIter.end(); ++pkgIter) {
        // Auto-removable packages are marked as garbage in the cache
        if (!cache[pkgIter].Garbage)
            continue;

        isResidual = pkgIter->CurrentState == pkgCache::State::ConfigFiles;
        // Delete auto-removable packages, but we can't delete residual packages
        if (pkgIter.CurrentVer() && !isResidual)
            cache.MarkDelete(pkgIter, false);
    }

    emit packageChanged();
}

void Backend::markPackageForInstall(const QString &name)
{
    Package *pkg = package(name);
    pkg->setInstall();
}

void Backend::markPackageForRemoval(const QString &name)
{
    Package *pkg = package(name);
    pkg->setRemove();
}

void Backend::markPackages(const QApt::PackageList &packages, QApt::Package::State action)
{
    Q_D(Backend);

    if (packages.isEmpty()) {
        return;
    }

    pkgDepCache *deps = d->cache->depCache();
    setCompressEvents(true);

    foreach (Package *package, packages) {
        const pkgCache::PkgIterator &iter = package->packageIterator();
        switch (action) {
        case Package::ToInstall: {
            int state = package->staticState();
            // Mark for install if not already installed, or if upgradeable
            if (!(state & Package::Installed) || (state & Package::Upgradeable)) {
                package->setInstall();
            }
            break;
        }
        case Package::ToRemove:
            if (package->isInstalled()) {
                package->setRemove();
            }
            break;
        case Package::ToUpgrade: {
            bool fromUser = !(package->state() & Package::IsAuto);
            deps->MarkInstall(iter, true, 0, fromUser);
            break;
        }
        case Package::ToReInstall: {
            int state = package->staticState();

            if(state & Package::Installed
               && !(state & Package::NotDownloadable)
               && !(state & Package::Upgradeable)) {
                package->setReInstall();
            }
            break;
        }
        case Package::ToKeep:
            package->setKeep();
            break;
        case Package::ToPurge: {
            int state = package->staticState();

            if ((state & Package::Installed) || (state & Package::ResidualConfig)) {
                package->setPurge();
            }
            break;
        }
        default:
            break;
        }
    }

    setCompressEvents(false);
    emit packageChanged();
}

void Backend::setCompressEvents(bool enabled)
{
    Q_D(Backend);

    if (enabled) {
         // Ignore if already compressed
        if (d->actionGroup != nullptr)
            return;

        // Presence of an ActionGroup compresses marking events over its lifetime
        d->actionGroup = new pkgDepCache::ActionGroup(*d->cache->depCache());
    } else {
        delete d->actionGroup;
        d->actionGroup = nullptr;
        emit packageChanged();
    }
}

QApt::Transaction * Backend::commitChanges()
{
    Q_D(Backend);

    QVariantMap packageList;
    for (const Package *package : d->packages) {
        int flags = package->state();
        std::string fullName = package->packageIterator().FullName();
        // Cannot have any of these flags simultaneously
        int status = flags & (Package::IsManuallyHeld |
                              Package::NewInstall |
                              Package::ToReInstall |
                              Package::ToUpgrade |
                              Package::ToDowngrade |
                              Package::ToRemove);
        switch (status) {
        case Package::IsManuallyHeld:
            packageList.insert(QString::fromStdString(fullName), Package::Held);
            break;
        case Package::NewInstall:
            if (!(flags & Package::IsAuto)) {
                packageList.insert(QString::fromStdString(fullName), Package::ToInstall);
            }
            break;
        case Package::ToReInstall:
            packageList.insert(QString::fromStdString(fullName), Package::ToReInstall);
            break;
        case Package::ToUpgrade:
            packageList.insert(QString::fromStdString(fullName), Package::ToUpgrade);
            break;
        case Package::ToDowngrade:
            packageList.insert(QString(QString::fromStdString(fullName)) % ',' % package->availableVersion(), Package::ToDowngrade);
            break;
        case Package::ToRemove:
            if(flags & Package::ToPurge) {
                packageList.insert(QString::fromStdString(fullName), Package::ToPurge);
            } else {
                packageList.insert(QString::fromStdString(fullName), Package::ToRemove);
            }
            break;
        }
    }

    QDBusPendingReply<QString> rep = d->worker->commitChanges(packageList);
    Transaction *trans = new Transaction(rep.value());
    trans->setFrontendCaps(d->frontendCaps);

    return trans;
}

QApt::Transaction * Backend::installPackages(PackageList packages)
{
    Q_D(Backend);

    QVariantMap packageList;

    for (const Package *package : packages) {
        std::string fullName = package->packageIterator().FullName();
        packageList.insert(QString::fromStdString(fullName), Package::ToInstall);
    }

    QDBusPendingReply<QString> rep = d->worker->commitChanges(packageList);
    Transaction *trans = new Transaction(rep.value());
    trans->setFrontendCaps(d->frontendCaps);

    return trans;
}

QApt::Transaction * Backend::removePackages(PackageList packages)
{
    Q_D(Backend);

    QVariantMap packageList;

    for (const Package *package : packages) {
        std::string fullName = package->packageIterator().FullName();
        packageList.insert(QString::fromStdString(fullName), Package::ToRemove);
    }

    QDBusPendingReply<QString> rep = d->worker->commitChanges(packageList);
    Transaction *trans = new Transaction(rep.value());
    trans->setFrontendCaps(d->frontendCaps);

    return trans;
}

Transaction *Backend::downloadArchives(const QString &listFile, const QString &destination)
{
    Q_D(Backend);

    QFile file(listFile);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return nullptr;
    }

    QByteArray buffer = file.readAll();

    QList<QByteArray> lines = buffer.split('\n');

    if (lines.isEmpty() || lines.first() != QByteArray("[Download List]")) {
        return nullptr;
    }

    lines.removeAt(0);

    QStringList packages;
    foreach (const QByteArray &line, lines) {
        packages << line;
    }

    QString dirName = listFile.left(listFile.lastIndexOf('/'));

    QDir dir(dirName);
    dir.mkdir(QLatin1String("packages"));

    QDBusPendingReply<QString> rep = d->worker->downloadArchives(packages, destination);
    Transaction *trans = new Transaction(rep.value());
    trans->setFrontendCaps(d->frontendCaps);

    return trans;
}

Transaction *Backend::installFile(const DebFile &debFile)
{
    Q_D(Backend);

    QDBusPendingReply<QString> rep = d->worker->installFile(debFile.filePath());
    Transaction *trans = new Transaction(rep.value());
    trans->setFrontendCaps(d->frontendCaps);

    return trans;
}

void Backend::emitPackageChanged()
{
    emit packageChanged();
}

Transaction *Backend::updateCache()
{
    Q_D(Backend);

    QDBusPendingReply<QString> rep = d->worker->updateCache();
    Transaction *trans = new Transaction(rep.value());
    trans->setFrontendCaps(d->frontendCaps);

    return trans;
}

Transaction *Backend::upgradeSystem(UpgradeType upgradeType)
{
    Q_D(Backend);

    bool safeUpgrade = (upgradeType == QApt::SafeUpgrade);
    QDBusPendingReply<QString> rep = d->worker->upgradeSystem(safeUpgrade);
    Transaction *trans = new Transaction(rep.value());
    trans->setFrontendCaps(d->frontendCaps);

    return trans;
}

bool Backend::saveInstalledPackagesList(const QString &path) const
{
    Q_D(const Backend);

    QString selectionDocument;
    for (int i = 0; i < d->packages.size(); ++i) {

        if (d->packages.at(i)->isInstalled()) {
            selectionDocument.append(d->packages[i]->name() %
            QLatin1Literal("\t\tinstall") % QLatin1Char('\n'));
        }
    }

    if (selectionDocument.isEmpty()) {
        return false;
    }

    return d->writeSelectionFile(selectionDocument, path);
}

bool Backend::saveSelections(const QString &path) const
{
    Q_D(const Backend);

    QString selectionDocument;
    for (int i = 0; i < d->packages.size(); ++i) {
        int flags = d->packages.at(i)->state();

        if (flags & Package::ToInstall) {
            selectionDocument.append(d->packages[i]->name() %
            QLatin1Literal("\t\tinstall") % QLatin1Char('\n'));
        } else if (flags & Package::ToRemove) {
            selectionDocument.append(d->packages[i]->name() %
            QLatin1Literal("\t\tdeinstall") % QLatin1Char('\n'));
        }
    }

    if (selectionDocument.isEmpty()) {
        return false;
    }

    return d->writeSelectionFile(selectionDocument, path);
}

bool Backend::loadSelections(const QString &path)
{
    Q_D(Backend);

    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }

    int lineIndex = 0;
    QByteArray buffer = file.readAll();

    QList<QByteArray> lines = buffer.split('\n');
    QHash<QByteArray, int> actionMap;

    while (lineIndex < lines.size()) {
        QByteArray line = lines.at(lineIndex);
        if (line.isEmpty() || line.at(0) == '#') {
            lineIndex++;
            continue;
        }

        line = line.trimmed();

        QByteArray aKey;
        std::string keyString;
        const char *data = line.constData();
        if (ParseQuoteWord(data, keyString) == false)
            return false;
        aKey = QByteArray(keyString.c_str());

        QByteArray aValue;
        std::string valueString;
        if (ParseQuoteWord(data, valueString) == false)
            return false;
        aValue = QByteArray(valueString.c_str());

        if (aValue.at(0) == 'i') {
            actionMap[aKey] = Package::ToInstall;
        } else if ((aValue.at(0) == 'd') || (aValue.at(0) == 'u') || (aValue.at(0) == 'r')) {
            actionMap[aKey] = Package::ToRemove;
        } else if (aValue.at(0) == 'p') {
            actionMap[aKey] = Package::ToPurge;
        }

        ++lineIndex;
    }

    if (actionMap.isEmpty()) {
       return false;
    }

    pkgDepCache &cache = *d->cache->depCache();
    // Should protect whatever is already selected in the cache.
    pkgProblemResolver Fix(&cache);

    pkgCache::PkgIterator pkgIter;
    auto mapIter = actionMap.constBegin();
    while (mapIter != actionMap.constEnd()) {
        pkgIter = d->cache->depCache()->FindPkg(mapIter.key().constData());
        if (pkgIter.end()) {
            return false;
        }

        Fix.Clear(pkgIter);
        Fix.Protect(pkgIter);

        switch (mapIter.value()) {
           case Package::ToInstall:
               if (pkgIter.CurrentVer().end()) { // Only mark if not already installed
                  cache.MarkInstall(pkgIter, true);
               }
               break;
           case Package::ToRemove:
               Fix.Remove(pkgIter);
               cache.MarkDelete(pkgIter, false);
               break;
           case Package::ToPurge:
               Fix.Remove(pkgIter);
               cache.MarkDelete(pkgIter, true);
               break;
        }
        ++mapIter;
    }

    Fix.Resolve(true);

    emit packageChanged();

    return true;
}

bool Backend::saveDownloadList(const QString &path) const
{
    Q_D(const Backend);

    QString downloadDocument;
    downloadDocument.append(QLatin1String("[Download List]") % QLatin1Char('\n'));
    for (int i = 0; i < d->packages.size(); ++i) {
        int flags = d->packages.at(i)->state();

        if (flags & Package::ToInstall) {
            downloadDocument.append(d->packages[i]->name() % QLatin1Char('\n'));
        }
    }

    return d->writeSelectionFile(downloadDocument, path);
}

bool Backend::setPackagePinned(Package *package, bool pin)
{
    Q_D(Backend);

    QString dir = d->config->findDirectory("Dir::Etc") % QLatin1String("preferences.d/");
    QString path = dir % package->name();
    QString pinDocument;

    if (pin) {
        if (package->state() & Package::IsPinned) {
            return true;
        }

        pinDocument = QLatin1Literal("Package: ") % package->name()
                      % QLatin1Char('\n');

        if (package->installedVersion().isEmpty()) {
            pinDocument += QLatin1String("Pin: version  0.0\n");
        } else {
            pinDocument += QLatin1Literal("Pin: version ") % package->installedVersion()
                           % QLatin1Char('\n');
        }

        // Make configurable?
        pinDocument += QLatin1String("Pin-Priority: 1001\n\n");
    } else {
        QDir logDirectory(dir);
        QStringList pinFiles = logDirectory.entryList(QDir::Files, QDir::Name);
        pinFiles << QString::fromStdString(_config->FindDir("Dir::Etc")) %
                    QLatin1String("preferences");

        // Search all pin files, delete package stanza from file
        for (const QString &pinName : pinFiles) {
            QString pinPath;
            if (!pinName.startsWith(QLatin1Char('/'))) {
                pinPath = dir % pinName;
            } else {
                pinPath = pinName;
            }

            if (!QFile::exists(pinPath))
                continue;

            // Open to get a file name
            QTemporaryFile tempFile;
            if (!tempFile.open()) {
                return false;
            }
            tempFile.close();

            QString tempFileName = tempFile.fileName();
            FILE *out = fopen(tempFileName.toUtf8(), "w");
            if (!out) {
                return false;
            }

            FileFd Fd(pinPath.toUtf8().data(), FileFd::ReadOnly);

            pkgTagFile tagFile(&Fd);
            if (_error->PendingError()) {
                fclose(out);
                return false;
            }

            pkgTagSection tags;
            while (tagFile.Step(tags)) {
                QString name = QLatin1String(tags.FindS("Package").c_str());

                if (name.isEmpty()) {
                    return false;
                }

                // Include all but the matching name in the new pinfile
                if (name != package->name()) {
                    TFRewriteData tfrd;
                    tfrd.Tag = 0;
                    tfrd.Rewrite = 0;
                    tfrd.NewTag = 0;
                    TFRewrite(out, tags, TFRewritePackageOrder, &tfrd);
                    fprintf(out, "\n");
                }
            }
            fclose(out);

            if (!tempFile.open()) {
                return false;
            }

            pinDocument = tempFile.readAll();
        }
    }

    if (!d->worker->writeFileToDisk(pinDocument, path)) {
        return false;
    }

    return true;
}

void Backend::updateXapianIndex()
{
    QDBusMessage m = QDBusMessage::createMethodCall(QLatin1String("org.debian.AptXapianIndex"),
                                                    QLatin1String("/"),
                                                    QLatin1String("org.debian.AptXapianIndex"),
                                                    QLatin1String("update_async"));
    QVariantList dbusArgs;

    dbusArgs << /*force*/ true << /*update_only*/ true;
    m.setArguments(dbusArgs);
    QDBusConnection::systemBus().send(m);

    QDBusConnection::systemBus().connect(QLatin1String("org.debian.AptXapianIndex"),
                                         QLatin1String("/"),
                                         QLatin1String("org.debian.AptXapianIndex"),
                                         QLatin1String("UpdateProgress"),
                                         this, SIGNAL(xapianUpdateProgress(int)));
    QDBusConnection::systemBus().connect(QLatin1String("org.debian.AptXapianIndex"),
                                         QLatin1String("/"),
                                         QLatin1String("org.debian.AptXapianIndex"),
                                         QLatin1String("UpdateFinished"),
                                         this, SLOT(emitXapianUpdateFinished()));
    emit xapianUpdateStarted();
}

void Backend::emitXapianUpdateFinished()
{
    QDBusConnection::systemBus().disconnect(QLatin1String("org.debian.AptXapianIndex"),
                                            QLatin1String("/"),
                                            QLatin1String("org.debian.AptXapianIndex"),
                                            QLatin1String("UpdateProgress"),
                                            this, SIGNAL(xapianUpdateProgress(int)));
    QDBusConnection::systemBus().disconnect(QLatin1String("org.debian.AptXapianIndex"),
                                            QLatin1String("/"),
                                            QLatin1String("org.debian.AptXapianIndex"),
                                            QLatin1String("UpdateFinished"),
                                            this, SLOT(xapianUpdateFinished(bool)));
    openXapianIndex();
    emit xapianUpdateFinished();
}

bool Backend::addArchiveToCache(const DebFile &archive)
{
    Q_D(Backend);

    // Sanity checks
    Package *pkg = package(archive.packageName());
    if (!pkg) {
        // The package is not in the cache, so we can't do anything
        // with this .deb
        return false;
    }

    QString arch = archive.architecture();

    if (arch != QLatin1String("all") &&
        arch != d->config->readEntry(QLatin1String("APT::Architecture"), QString())) {
        // Incompatible architecture
        return false;
    }

    QString debVersion = archive.version();
    QString candVersion = pkg->availableVersion();

    if (debVersion != candVersion) {
        // Incompatible version
        return false;
    }

    if (archive.md5Sum() != pkg->md5Sum()) {
        // Not the same as the candidate
        return false;
    }

    // Add the package, but we'll need auth so the worker'll do it
    return d->worker->copyArchiveToCache(archive.filePath());
}

void Backend::setFrontendCaps(FrontendCaps caps)
{
    Q_D(Backend);

    d->frontendCaps = caps;
}

}
