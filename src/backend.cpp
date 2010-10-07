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

#include "backend.h"

// Qt includes
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

// Apt includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>

// Xapian includes
#include <xapian.h>

// QApt includes
#include "cache.h"
#include "workerdbus.h" // OrgKubuntuQaptworkerInterface

namespace QApt {

class BackendPrivate
{
public:
    BackendPrivate() : cache(0), records(0) {}
    ~BackendPrivate()
    {
        delete cache;
        delete records;
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

    // Counts
    int installedCount;

    // Pointer to the apt cache object
    Cache *cache;
    pkgPolicy *policy;
    pkgRecords *records;

    // Undo/redo stuff
    QList<CacheState> undoStack;
    QList<CacheState> redoStack;

    // Xapian
    time_t xapianTimeStamp;
    Xapian::Database xapianDatabase;

    // DBus
    QDBusServiceWatcher *watcher;
    OrgKubuntuQaptworkerInterface *worker;
};

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
    if (!pkgInitConfig(*_config)) {
        return false;
    }

    if (!pkgInitSystem(*_config, _system)) {
        return false;
    }

    d->cache = new Cache(this);
    reloadCache();
    openXapianIndex();

    return true;
}

void Backend::reloadCache()
{
    Q_D(Backend);

    if (!d->cache->open()) {
        QVariantMap details;
        string message;
        bool isError = _error->PopMessage(message);
        if (isError) {
            details[QLatin1String("ErrorText")] = QString::fromStdString(message);
        }
        emitErrorOccurred(QApt::InitError, details);
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

    // Remove check when sid has >= 4.7
    #if QT_VERSION >= 0x040700
    d->packages.reserve(packageCount);
    #endif

    // Populate internal package cache
    int count = 0;
    QSet<Group> groupSet;
    QStringList originList;
    QStringList originLabelList;

    pkgCache::PkgIterator iter;
    for (iter = depCache->PkgBegin(); !iter.end(); ++iter) {
        if (iter->VersionList == 0) {
            continue; // Exclude virtual packages.
        }

        Package *pkg = new Package(this, depCache, d->records, iter);
        d->packagesIndex[iter->ID] = count;
        d->packages.insert(count++, pkg);

        if (iter->CurrentVer) {
            d->installedCount++;
        }

        QString group = pkg->section();

        if (!group.isEmpty()) {
            groupSet << group;
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

    // Populate groups
    foreach (const QString &group, groupSet) {
        d->groups << group;
    }

    d->undoStack.clear();
    d->redoStack.clear();
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
    Q_D(const Backend);

    pkgCache::PkgIterator pkg = d->cache->depCache()->FindPkg(name.toStdString());
    if (!pkg.end()) {
        return package(pkg);
    }
    return 0;
}

Package *Backend::packageForFile(const QString &file) const
{
    Q_D(const Backend);

    foreach (Package *package, d->packages) {
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

    QString originLabel = d->originMap[origin];

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

    int packageCount = d->packages.size();

    return packageCount;
}

int Backend::packageCount(const Package::States &states) const
{
    Q_D(const Backend);

    int packageCount = 0;

    foreach(const Package *package, d->packages) {
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

    qint64 downloadSize = d->cache->depCache()->DebSize();

    pkgAcquire fetcher;
    pkgPackageManager *PM = _system->CreatePM(d->cache->depCache());
    if (PM->GetArchives(&fetcher, d->cache->list(), d->records)) {
        downloadSize = fetcher.FetchNeeded();
    }
    delete PM;

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

    foreach (Package *package, d->packages) {
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

    foreach(Package *package, d->packages) {
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
    static int qualityCutoff = 25;
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
         Package* pkg = package(QString::fromStdString(i.get_document().get_data()));
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

bool Backend::isBroken() const
{
    Q_D(const Backend);

    if (d->cache->depCache() == 0) {
        return true;
    }

    // Check for broken things
    if (d->cache->depCache()->BrokenCount() != 0) {
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
   if (d->xapianTimeStamp < aptCacheTime.toTime_t() ||
       d->xapianTimeStamp < dpkgStatusTime.toTime_t()) {
      return true;
   }

   return false;
}

bool Backend::openXapianIndex()
{
    Q_D(Backend);

    QFileInfo timeStamp(QLatin1String("/var/lib/apt-xapian-index/update-timestamp"));
    d->xapianTimeStamp = timeStamp.lastModified().toTime_t();

    try {
        d->xapianDatabase.add_database(Xapian::Database("/var/lib/apt-xapian-index/index"));
    } catch (Xapian::DatabaseOpeningError) {
        return false;
    };

    return true;
}

CacheState Backend::currentCacheState() const
{
    Q_D(const Backend);

    CacheState state;
    int pkgSize = d->packages.size();
    #if QT_VERSION >= 0x040700
    state.reserve(pkgSize);
    #endif
    for (unsigned i = 0; i < pkgSize; ++i) {
        state.append(d->packages[i]->state());
    }

    return state;
}

void Backend::saveCacheState()
{
    Q_D(Backend);
    CacheState state = currentCacheState();
    d->undoStack.prepend(state);
    d->redoStack.clear();

    // TODO: Configurable
    int maxStackSize = 20;

    while (d->undoStack.size() > maxStackSize) {
        d->undoStack.removeLast();
    }
}

void Backend::restoreCacheState(const CacheState &state)
{
    Q_D(Backend);

    pkgDepCache *deps = d->cache->depCache();
    pkgDepCache::ActionGroup group(*deps);

    for (unsigned i = 0; i < d->packages.size(); ++i) {
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
            if(pkgIter.CurrentVer() != 0 &&
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

    foreach (const Package *package, d->packages) {
        int flags = package->state();
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
                   packageList.insert(package->name(), Package::Held);
               }
               break;
           case Package::NewInstall:
               packageList.insert(package->name(), Package::ToInstall);
               break;
           case Package::ToReInstall:
               packageList.insert(package->name(), Package::ToReInstall);
               break;
           case Package::ToUpgrade:
               packageList.insert(package->name(), Package::ToUpgrade);
               break;
           case Package::ToDowngrade:
               packageList.insert(package->name() % ',' % package->availableVersion(), Package::ToDowngrade);
               break;
           case Package::ToRemove:
               if(flags & Package::ToPurge) {
                   packageList.insert(package->name(), Package::ToPurge);
               } else {
                   packageList.insert(package->name(), Package::ToRemove);
               }
               break;
        }
    }

    d->worker->setLocale(QLatin1String(setlocale(LC_ALL, 0)));
    d->worker->commitChanges(packageList);
}

void Backend::packageChanged(Package *package)
{
    emit packageChanged();
}

void Backend::updateCache()
{
    Q_D(Backend);

    d->worker->setLocale(QLatin1String(setlocale(LC_ALL, 0)));
    d->worker->updateCache();
}

bool Backend::saveSelections(const QString &path) const
{
    Q_D(const Backend);

    QString selectionDocument;
    for (unsigned i = 0; i < d->packages.size(); ++i) {
        int flags = d->packages[i]->state();

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

    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        return false;
    } else {
        QTextStream out(&file);
        out << selectionDocument;
    }

    return true;
}

bool Backend::loadSelections(const QString &path)
{
    Q_D(Backend);

    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return false;
    }

    QTextStream in(&file);

    QString line;

    string stdName;
    string stdAction;
    QString packageName;
    QString packageAction;

    QHash<QString, int> actionMap;
    do {
        line = in.readLine();
        if (line.isEmpty() || line.at(0) == QLatin1Char('#')) {
            continue;
        }
        line = line.simplified();

        const char *C = line.toStdString().data();
        if (!ParseQuoteWord(C, stdName)) {
            return false;
        }
        packageName = QString::fromStdString(stdName);

        if (!ParseQuoteWord(C, stdAction)) {
            return false;
        }
        packageAction = QString::fromStdString(stdAction);

        if (packageAction.at(0) == QLatin1Char('i')) {
            actionMap[packageName] = Package::ToInstall;
        } else if ((packageAction.at(0) == QLatin1Char('d')) || (packageAction.at(0) == QLatin1Char('u'))) {
            actionMap[packageName] = Package::ToRemove;
        }
    } while (!line.isNull());

    if (actionMap.isEmpty()) {
       return false;
    }

    pkgDepCache &cache = *d->cache->depCache();
    pkgProblemResolver Fix(&cache);
    // XXX Should protect whatever is already selected in the cache.

    pkgCache::PkgIterator pkgIter;
    QHash<QString, int>::const_iterator mapIter = actionMap.begin();
    while (mapIter != actionMap.end()) {
        pkgIter = d->cache->depCache()->FindPkg(mapIter.key().toStdString());
        if (pkgIter.end()) {
            return false;
        }

        Fix.Clear(pkgIter);
        Fix.Protect(pkgIter);

        switch (mapIter.value()) {
           case Package::ToInstall:
               if(_config->FindB("Volatile::SetSelectionDoReInstall",false)) {
                   cache.SetReInstall(pkgIter, true);
               }
               if(_config->FindB("Volatile::SetSelectionsNoFix",false)) {
                   cache.MarkInstall(pkgIter, false);
               } else {
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

    return true;
}

void Backend::updateXapianIndex()
{
    Q_D(Backend);

    d->worker->updateXapianIndex();
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
    connect(d->worker, SIGNAL(questionOccurred(int, const QVariantMap&)),
            this, SLOT(emitWorkerQuestionOccurred(int, const QVariantMap&)));
    connect(d->worker, SIGNAL(warningOccurred(int, const QVariantMap&)),
            this, SLOT(emitWarningOccurred(int, const QVariantMap&)));
}

void Backend::workerFinished(bool result)
{
    Q_D(Backend);

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
    emit workerEvent((WorkerEvent) event);
}

void Backend::emitWorkerQuestionOccurred(int question, const QVariantMap &details)
{
    emit questionOccurred((WorkerQuestion) question, details);
}

void Backend::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    if (oldOwner.isEmpty()) {
        return; // Don't care, just appearing
    }

    if (newOwner.isEmpty()) {
        // Ok, something got screwed. Report and flee
        emit errorOccurred(QApt::WorkerDisappeared, QVariantMap());
        workerFinished(false);
    }
}

}
