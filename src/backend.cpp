/***************************************************************************
 *   Copyright © 2010-2011 Jonathan Thomas <echidnaman@kubuntu.org>        *
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
#include <QtCore/QStringList>
#include <QtCore/QTemporaryFile>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

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
#include <xapian.h>

// QApt includes
#include "cache.h"
#include "config.h"
#include "debfile.h"
#include "workerdbus.h" // OrgKubuntuQaptworkerInterface

namespace QApt {

class BackendPrivate
{
public:
    BackendPrivate()
        : state(InvalidEvent)
        , cache(0)
        , records(0)
        , maxStackSize(20)
        , config(0)
    {
    }
    ~BackendPrivate()
    {
        delete cache;
        delete records;
        delete config;
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
    WorkerEvent state;

    // Counts
    int installedCount;

    // Pointer to the apt cache object
    Cache *cache;
    pkgPolicy *policy;
    pkgRecords *records;

    // Undo/redo stuff
    int maxStackSize;
    QList<CacheState> undoStack;
    QList<CacheState> redoStack;

    // Xapian
    time_t xapianTimeStamp;
    Xapian::Database xapianDatabase;
    bool xapianIndexExists;

    // DBus
    QDBusServiceWatcher *watcher;
    OrgKubuntuQaptworkerInterface *worker;

    Config *config;
    bool isMultiArch;
    QString nativeArch;

    bool writeSelectionFile(const QString &file, const QString &path) const;
    void setWorkerLocale();
    void setWorkerProxy();
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

void BackendPrivate::setWorkerLocale()
{
    worker->setLocale(QLatin1String(setlocale(LC_MESSAGES, 0)));
}

void BackendPrivate::setWorkerProxy()
{
    worker->setProxy(QLatin1String(getenv("http_proxy")));
}

Backend::Backend()
        : d_ptr(new BackendPrivate)
{
    Q_D(Backend);

    d->worker = new OrgKubuntuQaptworkerInterface(QLatin1String("org.kubuntu.qaptworker"),
                                                  QLatin1String("/"), QDBusConnection::systemBus(),
                                                  this);

    connect(d->worker, SIGNAL(errorOccurred(int, const QVariantMap&)),
            this, SLOT(emitErrorOccurred(int, const QVariantMap&)));
    connect(d->worker, SIGNAL(workerStarted()), this, SLOT(workerStarted()));
    connect(d->worker, SIGNAL(workerEvent(int)), this, SLOT(emitWorkerEvent(int)));
    connect(d->worker, SIGNAL(workerFinished(bool)), this, SLOT(workerFinished(bool)));
    connect(d->worker, SIGNAL(xapianUpdateProgress(int)), this, SIGNAL(xapianUpdateProgress(int)));

    d->watcher = new QDBusServiceWatcher(this);
    d->watcher->setConnection(QDBusConnection::systemBus());
    d->watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    d->watcher->addWatchedService(QLatin1String("org.kubuntu.qaptworker"));
}

Backend::~Backend()
{
    delete d_ptr;
}

bool Backend::init()
{
    Q_D(Backend);
    if (!pkgInitConfig(*_config) || !pkgInitSystem(*_config, _system)) {
        throwInitError();
        return false;
    }

    d->cache = new Cache(this);
    d->config = new Config(this);
    d->nativeArch = config()->readEntry(QLatin1String("APT::Architecture"),
                                        QLatin1String(""));
    reloadCache();
    openXapianIndex();

    return true;
}

void Backend::reloadCache()
{
    Q_D(Backend);

    if (!d->cache->open()) {
        throwInitError();
        return;
    }

    pkgDepCache *depCache = d->cache->depCache();

    delete d->records;
    d->records = new pkgRecords(*depCache);

    qDeleteAll(d->packages);
    d->packages.clear();
    d->groups.clear();
    d->originMap.clear();
    d->packagesIndex.clear();
    d->installedCount = 0;

    int packageCount = depCache->Head().PackageCount;
    d->packagesIndex.resize(packageCount);
    d->packages.reserve(packageCount);

    // Populate internal package cache
    int count = 0;
    QSet<Group> groupSet;

    d->isMultiArch = architectures().size() > 1;
    bool pkgMultiArch;
    QString arch;

    pkgCache::PkgIterator iter;
    for (iter = depCache->PkgBegin(); !iter.end(); ++iter) {
        if (!iter->VersionList) {
            continue; // Exclude virtual packages.
        }

        Package *pkg = new Package(this, depCache, d->records, iter);

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
            pkgCache::VerFileIterator VF = Ver.FileList();
            d->originMap[QLatin1String(VF.File().Origin())] = QLatin1String(VF.File().Label());
        }
    }

    if (d->originMap.contains(QLatin1String(""))) {
        d->originMap.remove(QLatin1String(""));
    }

    d->undoStack.clear();
    d->redoStack.clear();

    // Determine which packages are pinned for display purposes
    QString dir = QString::fromStdString(_config->FindDir("Dir::Etc"))
                  % QLatin1String("preferences.d/");
    QDir logDirectory(dir);
    QStringList pinFiles = logDirectory.entryList(QDir::Files, QDir::Name);
    pinFiles << QString::fromStdString(_config->FindDir("Dir::Etc")) %
                QLatin1String("preferences");

    Q_FOREACH (const QString &pinName, pinFiles) {
        QString pinPath;

        // Handle absolute paths
        if (!pinName.startsWith(QLatin1Char('/'))) {
            pinPath = dir % pinName;
        } else {
            pinPath = pinName;
        }
        QFile pinFile(pinPath);

        if (!pinFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        FileFd Fd(pinFile.handle(), FileFd::ReadOnly);

        pkgTagFile tagFile(&Fd);
        if (_error->PendingError()) {
            continue;
        }

        pkgTagSection tags;
        while (tagFile.Step(tags)) {
            QLatin1String name(tags.FindS("Package").c_str());
            Package *pkg = package(name);
            if (pkg) {
                pkg->setPinned(true);
            }
        }
    }
}

void Backend::throwInitError()
{
    QVariantMap details;
    string message;
    bool isError = _error->PopMessage(message);
    if (isError) {
        details[QLatin1String("FromWorker")] = false;
        details[QLatin1String("ErrorText")] = QString::fromStdString(message);
    }

    emitErrorOccurred(QApt::InitError, details);
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

Package *Backend::package(pkgCache::PkgIterator &iter) const
{
    Q_D(const Backend);

    int index = d->packagesIndex.at(iter->ID);
    if (index != -1) {
        return d->packages.at(index);
    }
    return 0;
}

Package *Backend::package(const QString &name) const
{
    return package(QLatin1String(name.toStdString().c_str()));
}

Package *Backend::package(const QLatin1String &name) const
{
    Q_D(const Backend);

    pkgCache::PkgIterator pkg = d->cache->depCache()->FindPkg(name.latin1());
    if (!pkg.end()) {
        return package(pkg);
    }
    return 0;
}

Package *Backend::packageForFile(const QString &file) const
{
    Q_D(const Backend);

    if (file.isEmpty()) {
        return 0;
    }

    Q_FOREACH (Package *package, d->packages) {
        if (package->installedFilesList().contains(file)) {
            return package;
        }
    }
    return 0;
}

QStringList Backend::originLabels() const
{
    Q_D(const Backend);

    return d->originMap.values();
}

QString Backend::originLabel(const QString &origin) const
{
    Q_D(const Backend);

    QString originLabel = d->originMap.value(origin);

    return originLabel;
}

QString Backend::origin(QString originLabel) const
{
    Q_D(const Backend);

    QString origin = d->originMap.key(originLabel);

    return origin;
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

    Q_FOREACH(const Package *package, d->packages) {
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

    Q_FOREACH (Package *package, d->packages) {
        if (package->state() & Package::Upgradeable) {
            upgradeablePackages << package;
        }
    }

    return upgradeablePackages;
}

PackageList Backend::markedPackages() const
{
    Q_D(const Backend);

    PackageList markedPackages;

    Q_FOREACH(Package *package, d->packages) {
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

    if (d->xapianTimeStamp == 0) {
        return QApt::PackageList();
    }

    string unsplitSearchString = searchString.toStdString();
    static int qualityCutoff = 15;
    PackageList searchResult;

    // Doesn't follow style guidelines to ease merging with synaptic
    try {
        int maxItems = d->xapianDatabase.get_doccount();
        Xapian::Enquire enquire(d->xapianDatabase);
        Xapian::QueryParser parser;
        parser.set_database(d->xapianDatabase);
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
         Package* pkg = package(QLatin1String(i.get_document().get_data().c_str()));
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
    std::vector<std::string> archs = APT::Configuration::getArchitectures(false);

    QStringList archList;
    for (std::vector<std::string>::const_iterator a = archs.begin();
         a != archs.end(); ++a)
    {
         archList.append(QString::fromStdString(*a));
    }


    return archList;
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

bool Backend::xapianIndexNeedsUpdate() const
{
    Q_D(const Backend);

   // If the cache has been modified after the xapian timestamp, we need to rebuild
   QDateTime aptCacheTime = QFileInfo(QLatin1String(_config->FindFile("Dir::Cache::pkgcache").c_str())).lastModified();
   QDateTime dpkgStatusTime = QFileInfo(QLatin1String("/var/lib/dpkg/status")).lastModified();

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

    try {
        d->xapianDatabase.add_database(Xapian::Database("/var/lib/apt-xapian-index/index"));
    } catch (Xapian::DatabaseOpeningError) {
        d->xapianIndexExists = false;
        return false;
    };

    d->xapianIndexExists = true;

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
        state.append(d->packages[i]->state());
    }

    return state;
}

WorkerEvent Backend::workerState() const
{
    Q_D(const Backend);

    return d->state;
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

    for (int i = 0; i < d->packages.size(); ++i) {
        Package *pkg = d->packages[i];
        int flags = pkg->state();
        int oldflags = state[i];

        if (oldflags != flags) {
            if (oldflags & Package::ToReInstall) {
                deps->MarkInstall(*(pkg->packageIterator()), true);
                deps->SetReInstall(*(pkg->packageIterator()), false);
            } else if (oldflags & Package::ToInstall) {
                deps->MarkInstall(*(pkg->packageIterator()), true);
            } else if (oldflags & Package::ToRemove) {
                deps->MarkDelete(*(pkg->packageIterator()), (bool)(oldflags & Package::ToPurge));
            } else if (oldflags & Package::ToKeep) {
                deps->MarkKeep(*(pkg->packageIterator()), false);
            }
            // fix the auto flag
            deps->MarkAuto(*pkg->packageIterator(), (oldflags & Package::IsAuto));
        }
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

    for (pkgCache::PkgIterator pkgIter = cache.PkgBegin(); !pkgIter.end(); ++pkgIter) {
        if (cache[pkgIter].Garbage) {
            if(pkgIter.CurrentVer() &&
               pkgIter->CurrentState != pkgCache::State::ConfigFiles) {
                cache.MarkDelete(pkgIter, false);
            }
        }

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

void Backend::commitChanges()
{
    Q_D(Backend);

    QVariantMap packageList;

    Q_FOREACH (const Package *package, d->packages) {
        int flags = package->state();
        std::string fullName = package->packageIterator()->FullName();
        // Cannot have any of these flags simultaneously
        int status = flags & (Package::ToKeep |
                              Package::NewInstall |
                              Package::ToReInstall |
                              Package::ToUpgrade |
                              Package::ToDowngrade |
                              Package::ToRemove);
        switch (status) {
           case Package::ToKeep:
               if (flags & Package::Held) {
                   packageList.insert(fullName.c_str(), Package::Held);
               }
               break;
           case Package::NewInstall:
               packageList.insert(fullName.c_str(), Package::ToInstall);
               break;
           case Package::ToReInstall:
               packageList.insert(fullName.c_str(), Package::ToReInstall);
               break;
           case Package::ToUpgrade:
               packageList.insert(fullName.c_str(), Package::ToUpgrade);
               break;
           case Package::ToDowngrade:
               packageList.insert(QString(fullName.c_str()) % ',' % package->availableVersion(), Package::ToDowngrade);
               break;
           case Package::ToRemove:
               if(flags & Package::ToPurge) {
                   packageList.insert(fullName.c_str(), Package::ToPurge);
               } else {
                   packageList.insert(fullName.c_str(), Package::ToRemove);
               }
               break;
        }
    }

    d->setWorkerLocale();
    d->setWorkerProxy();
    d->worker->commitChanges(packageList);
}

void Backend::downloadArchives(const QString &listFile, const QString &destination)
{
    Q_D(Backend);

    QFile file(listFile);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // TODO: error
        return;
    }

    QByteArray buffer = file.readAll();

    QList<QByteArray> lines = buffer.split('\n');

    if (lines.isEmpty() || lines.first() != QByteArray("[Download List]")) {
        return;
    }

    lines.removeAt(0);

    QStringList packages;
    foreach (const QByteArray &line, lines) {
        packages << line;
    }

    QString dirName = listFile.left(listFile.lastIndexOf('/'));

    QDir dir(dirName);
    dir.mkdir(QLatin1String("packages"));

    d->setWorkerLocale();
    d->setWorkerProxy();
    d->worker->downloadArchives(packages, destination);
}

void Backend::installDebFile(const DebFile &debFile)
{
    Q_D(Backend);

    d->setWorkerLocale();
    d->setWorkerProxy();
    d->worker->installDebFile(debFile.filePath());
}

void Backend::packageChanged(Package *package)
{
    Q_UNUSED(package);

    emit packageChanged();
}

void Backend::updateCache()
{
    Q_D(Backend);

    d->setWorkerLocale();
    d->setWorkerProxy();
    d->worker->updateCache();
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
        QByteArray aValue;
        int eqpos = line.indexOf("\t\t");

        if (eqpos < 0) {
            // Invalid
            lineIndex++;
            continue;
        } else {
            aKey = line.left(eqpos);
            aValue = line.right(line.size() - eqpos -2);
        }

        if (aValue.at(0) == 'i') {
            actionMap[aKey] = Package::ToInstall;
        } else if ((aValue.at(0) == 'd') || (aValue.at(0) == 'u')) {
            actionMap[aKey] = Package::ToRemove;
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
    QHash<QByteArray, int>::const_iterator mapIter = actionMap.begin();
    while (mapIter != actionMap.end()) {
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
        }
        ++mapIter;
    }

    Fix.InstallProtect();
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

    QString dir = QString::fromStdString(_config->FindDir("Dir::Etc"))
                  % QLatin1String("preferences.d/");
    QString path = dir % package->latin1Name();
    QString pinDocument;

    if (pin) {
        if (package->state() & Package::IsPinned) {
            return true;
        }

        pinDocument = QLatin1Literal("Package: ") % package->latin1Name()
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
        Q_FOREACH (const QString &pinName, pinFiles) {
            QString pinPath;
            if (!pinName.startsWith(QLatin1Char('/'))) {
                pinPath = dir % pinName;
            } else {
                pinPath = pinName;
            }
            QFile pinFile(pinPath);

            // Open to get a file name
            QTemporaryFile tempFile;
            if (!tempFile.open()) {
                return false;
            }
            tempFile.close();

            QString tempFileName = tempFile.fileName();
            FILE *out = fopen(tempFileName.toStdString().c_str(),"w");
            if (!out) {
                return false;
            }

            if (!pinFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            FileFd Fd(pinFile.handle(), FileFd::ReadOnly);

            pkgTagFile tagFile(&Fd);
            if (_error->PendingError()) {
                return false;
            }

            pkgTagSection tags;
            while (tagFile.Step(tags)) {
                QString name = QLatin1String(tags.FindS("Package").c_str());

                if (name.isEmpty()) {
                    return false;
                }

                // Include all but the matching name in the new pinfile
                if (name != package->latin1Name()) {
                    TFRewriteData tfrd;
                    tfrd.Tag = 0;
                    tfrd.Rewrite = 0;
                    tfrd.NewTag = 0;
                    TFRewrite(out, tags, TFRewritePackageOrder, &tfrd);
                    fprintf(out, "\n");
                }
            }

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
    Q_D(Backend);

    d->worker->updateXapianIndex();
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

void Backend::workerStarted()
{
    Q_D(Backend);

    connect(d->watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    connect(d->worker, SIGNAL(downloadProgress(int, int, int)),
            this, SIGNAL(downloadProgress(int, int, int)));
    connect(d->worker, SIGNAL(packageDownloadProgress(const QString&, int, const QString&, double, int)),
            this, SIGNAL(packageDownloadProgress(const QString&, int, const QString&, double, int)));
    connect(d->worker, SIGNAL(downloadMessage(int, const QString&)),
            this, SIGNAL(downloadMessage(int, const QString&)));
    connect(d->worker, SIGNAL(commitProgress(const QString&, int)),
            this, SIGNAL(commitProgress(const QString&, int)));
    connect(d->worker, SIGNAL(debInstallMessage(const QString&)),
            this, SIGNAL(debInstallMessage(const QString&)));
    connect(d->worker, SIGNAL(questionOccurred(int, const QVariantMap&)),
            this, SLOT(emitWorkerQuestionOccurred(int, const QVariantMap&)));
    connect(d->worker, SIGNAL(warningOccurred(int, const QVariantMap&)),
            this, SLOT(emitWarningOccurred(int, const QVariantMap&)));
}

void Backend::workerFinished(bool result)
{
    Q_D(Backend);

    Q_UNUSED(result);

    d->state = InvalidEvent;

    disconnect(d->watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
               this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    disconnect(d->worker, SIGNAL(downloadProgress(int, int, int)),
               this, SIGNAL(downloadProgress(int, int, int)));
    disconnect(d->worker, SIGNAL(packageDownloadProgress(const QString&, int, const QString&, double, int)),
               this, SIGNAL(packageDownloadProgress(const QString&, int, const QString&, double, int)));
    disconnect(d->worker, SIGNAL(downloadMessage(int, const QString&)),
               this, SIGNAL(downloadMessage(int, const QString&)));
    disconnect(d->worker, SIGNAL(commitProgress(const QString&, int)),
               this, SIGNAL(commitProgress(const QString&, int)));
    disconnect(d->worker, SIGNAL(debInstallMessage(const QString&)),
               this, SIGNAL(debInstallMessage(const QString&)));
    disconnect(d->worker, SIGNAL(questionOccurred(int, const QVariantMap&)),
               this, SLOT(emitWorkerQuestionOccurred(int, const QVariantMap&)));
    disconnect(d->worker, SIGNAL(warningOccurred(int, const QVariantMap&)),
               this, SLOT(emitWarningOccurred(int, const QVariantMap&)));
}

void Backend::cancelDownload()
{
    Q_D(Backend);

    d->worker->cancelDownload();
}

void Backend::answerWorkerQuestion(const QVariantMap &response)
{
    Q_D(Backend);

    d->worker->answerWorkerQuestion(response);
}

void Backend::emitErrorOccurred(int errorCode, const QVariantMap &details)
{
    emit errorOccurred((ErrorCode) errorCode, details);
}

void Backend::emitWarningOccurred(int warningCode, const QVariantMap &details)
{
    emit warningOccurred((WarningCode) warningCode, details);
}

void Backend::emitWorkerEvent(int event)
{
    Q_D(Backend);

    d->state = (WorkerEvent)event;
    emit workerEvent((WorkerEvent) event);
}

void Backend::emitWorkerQuestionOccurred(int question, const QVariantMap &details)
{
    emit questionOccurred((WorkerQuestion) question, details);
}

void Backend::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(name);

    if (oldOwner.isEmpty()) {
        return; // Don't care if empty, just appearing
    }

    if (newOwner.isEmpty()) {
        // Ok, something got screwed. Report and flee
        emit errorOccurred(QApt::WorkerDisappeared, QVariantMap());
        workerFinished(false);
    }
}

}
