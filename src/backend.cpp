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
#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

// Apt includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/init.h>

// Ept includes (xapian search)
#include <ept/textsearch/textsearch.h>

// QApt includes
#include "cache.h"
#include "workerdbus.h" // OrgKubuntuQaptworkerInterface

namespace ept {
namespace textsearch {
class TextSearch;
}
}

namespace QApt {

class BackendPrivate
{
public:
    BackendPrivate() : m_cache(0), m_records(0), textSearch(0) {}
    ~BackendPrivate()
    {
        delete m_cache;
        delete m_records;
    }
    // Watches DBus for our worker appearing/disappearing
    QDBusServiceWatcher *watcher;
    // The canonical list of all unique, non-virutal package objects
    PackageList packages;
    vector<int> packagesIndex;
    // Set of group names extracted from our packages
    QSet<Group*> groups;

    OrgKubuntuQaptworkerInterface *worker;

    // Pointer to the apt cache object
    Cache *m_cache;
    pkgPolicy *m_policy;
    pkgRecords *m_records;

    ept::textsearch::TextSearch *textSearch;
};

Backend::Backend()
        : d_ptr(new BackendPrivate)
{
    Q_D(Backend);

    d->worker = new OrgKubuntuQaptworkerInterface("org.kubuntu.qaptworker",
                                                  "/", QDBusConnection::systemBus(),
                                                  this);

    connect(d->worker, SIGNAL(errorOccurred(int, const QVariantMap&)),
            this, SLOT(emitErrorOccurred(int, const QVariantMap&)));
    connect(d->worker, SIGNAL(workerStarted()), this, SLOT(workerStarted()));
    connect(d->worker, SIGNAL(workerEvent(int)), this, SLOT(emitWorkerEvent(int)));
    connect(d->worker, SIGNAL(workerFinished(bool)), this, SLOT(workerFinished(bool)));

    d->watcher = new QDBusServiceWatcher(this);
    d->watcher->setConnection(QDBusConnection::systemBus());
    d->watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    d->watcher->addWatchedService("org.kubuntu.qaptworker");
}

Backend::~Backend()
{
}

bool Backend::init()
{
    Q_D(Backend);
    if (!pkgInitConfig(*_config)) {
        return false;
    }

    _config->Set("Initialized", 1);

    if (!pkgInitSystem(*_config, _system)) {
        return false;
    }

    d->m_cache = new Cache(this);
    reloadCache();
    openXapianIndex();

    return true;
}

void Backend::reloadCache()
{
    Q_D(Backend);

    d->m_cache->open();

    pkgDepCache *depCache = d->m_cache->depCache();

    delete d->m_records;
    d->m_records = new pkgRecords(*depCache);

    qDeleteAll(d->packages);
    qDeleteAll(d->groups);
    d->packages.clear();
    d->groups.clear();
    d->packagesIndex.clear();

    int packageCount = depCache->Head().PackageCount;
    d->packagesIndex.resize(packageCount, -1);

    // Populate internal package cache
    int count = 0;
    QSet<QString> groupSet;

    pkgCache::PkgIterator iter;
    for (iter = depCache->PkgBegin(); iter.end() != true; iter++) {
        if (iter->VersionList == 0) {
            continue; // Exclude virtual packages.
        }

        Package *pkg = new Package(this, depCache, d->m_records, iter);
        d->packagesIndex[iter->ID] = count;
        d->packages.insert(count++, pkg);

        if (iter.Section()) {
            QString name = QString::fromStdString(iter.Section());
            groupSet << name;
        }
    }

    // Populate groups
    foreach (const QString &groupName, groupSet) {
        Group *group = new Group(this, groupName);
        d->groups << group;
    }
}

pkgSourceList *Backend::packageSourceList()
{
    Q_D(const Backend);

    return d->m_cache->list();
}

Package *Backend::package(pkgCache::PkgIterator &iter) const
{
    Q_D(const Backend);
    int index = d->packagesIndex[iter->ID];
    if (index != -1) {
        return d->packages[index];
    }
    return 0;
}

Package *Backend::package(const QString &name) const
{
    Q_D(const Backend);

    pkgCache::PkgIterator pkg = d->m_cache->depCache()->FindPkg(name.toStdString());
    if (pkg.end() == false) {
        return package(pkg);
    }

    // FIXME: Need some type of fake package to return here if all else fails.
    // Otherwise, make sure you don't give this function invalid data.
    // Sucks, I know...
    qDebug() << "Porked!";
    return 0;
}

int Backend::packageCount()
{
    Q_D(const Backend);

    int packageCount = d->packages.size();

    return packageCount;
}

int Backend::packageCount(const Package::PackageStates &states) const
{
    Q_D(const Backend);

    int packageCount = 0;

    foreach(Package *package, d->packages) {
        if ((package->state() & states)) {
            packageCount++;
        }
    }

    return packageCount;
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
            if (package->state() & Package::Held) {
                continue; // Don't count held packages
            }
            upgradeablePackages << package;
        }
    }

    return upgradeablePackages;
}

PackageList Backend::search(const QString &unsplitSearchString) const
{
    Q_D(const Backend);

    string s;
    string originalSearchString = unsplitSearchString.toStdString();
    static int qualityCutoff = 25;
    static const int maxAltTerms = 15;
    PackageList searchResult;

    if(!d->textSearch || !d->textSearch->hasData()) {
        return searchResult;
    }

    Xapian::Enquire enquire(d->textSearch->db());
    Xapian::QueryParser parser;
    Xapian::Query query = parser.parse_query(unsplitSearchString.toStdString());
    enquire.set_query(query);

    // Get a set of tags to expand the query
    vector<string> expand = d->textSearch->expand(enquire);

    // now expand the query by adding the searching string as a package
    // name so that those searches appear erlier
    for (int i = 0; i < originalSearchString.size(); ++i) {
        if(isblank(originalSearchString[i])) {
            if(s.size() > 0) {
                expand.push_back("XP"+s);
            }
            s="";
        } else
            s+=originalSearchString[i];
    }

    // now add to the last found string
    expand.push_back("XP"+s);

    // the last string is always expanded to get better search as you
    // type results
    if (s.size() > 0) {
        Xapian::TermIterator I;
        int j = 0;

        for(I = d->textSearch->db().allterms_begin(s);
           I != d->textSearch->db().allterms_end(s);
           ++I) {
            expand.push_back(*I);
            expand.push_back("XP"+*I);
            // do not expand all alt terms, they can be huge > 100
            // and make the search very slow
            j++;
            if (j > maxAltTerms) {
                break;
            }
        }
    }

    // Build the expanded query
    Xapian::Query expansion(Xapian::Query::OP_OR, expand.begin(), expand.end());
    enquire.set_query(Xapian::Query(Xapian::Query::OP_OR, query, expansion));

    // Retrieve the results
    bool done = false;
    int top_percent = 0;
    for (size_t pos = 0; !done; pos += 20) {
        Xapian::MSet matches = enquire.get_mset(pos, 20);
        if (matches.size() < 20) {
            done = true;
        }
        for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i) {
            Package* pkg = package(QString::fromStdString(i.get_document().get_data()));
            // Filter out results that apt doesn't know
            if (!pkg) {
                continue;
            }

            // Save the confidence interval of the top value, to use it as
            // a reference to compute an adaptive quality cutoff
            if (top_percent == 0) {
                top_percent = i.get_percent();
            }

            // Stop producing if the quality goes below a cutoff point
            if (i.get_percent() < qualityCutoff * top_percent / 100) {
               done = true;
               break;
            }

            searchResult.append(pkg);
        }
    }

    return searchResult;
}

Group *Backend::group(const QString &name) const
{
    Q_D(const Backend);

    foreach (Group *group, d->groups) {
        if (group->name() == name) {
            return group;
        }
    }
}

GroupList Backend::availableGroups() const
{
    Q_D(const Backend);

    GroupList groupList = d->groups.toList();

    return groupList;
}

bool Backend::xapianIndexNeedsUpdate()
{
    Q_D(Backend);

    struct stat;
    struct buf;

    // check the xapian index
    if(FileExists("/usr/sbin/update-apt-xapian-index") &&
      (!d->textSearch || !d->textSearch->hasData())) {
        return true;
    }

   // compare timestamps, rebuild everytime, its now cheap(er)
   // because we use u-a-x-i --update
   QDateTime statTime;
   statTime = QFileInfo(_config->FindFile("Dir::Cache::pkgcache").c_str()).lastModified();
   if(d->textSearch->timestamp() < statTime.toTime_t()) {
      return true;
   }

   return false;
}

bool Backend::openXapianIndex()
{
    Q_D(Backend);

    if(d->textSearch) {
        delete d->textSearch;
    }

    try {
        d->textSearch = new ept::textsearch::TextSearch;
    } catch (Xapian::DatabaseOpeningError) {
        return false;
    };

    return true;
}

void Backend::markPackagesForUpgrade()
{
    Q_D(Backend);

    // TODO: Should say something if there's an error?
    pkgAllUpgrade(*d->m_cache->depCache());
}

void Backend::markPackagesForDistUpgrade()
{
    Q_D(Backend);

    // TODO: Should say something if there's an error?
    pkgDistUpgrade(*d->m_cache->depCache());
}

void Backend::markPackageForInstall(const QString &name)
{
    Package *pkg = package(name);
    pkg->setInstall();
}

void Backend::markPackageForRemoval(const QString &name, bool purge)
{
    Package *pkg = package(name);
    pkg->setRemove(purge);
}

void Backend::commitChanges()
{
    Q_D(Backend);

    QMap<QString, QVariant> instructionList;

    foreach (Package *package, d->packages) {
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
                   instructionList.insert(package->name(), Package::Held);
               }
               break;
           case Package::NewInstall:
               instructionList.insert(package->name(), Package::ToInstall);
               qDebug() << "Installing:" << package->name();
               break;
           case Package::ToReInstall:
               instructionList.insert(package->name(), Package::ToReInstall);
               break;
           case Package::ToUpgrade:
               instructionList.insert(package->name(), Package::ToUpgrade);
               qDebug() << "Upgrading:" << package->name();
               break;
           case Package::ToDowngrade:
               instructionList.insert(package->name(), Package::ToDowngrade);
               break;
           case Package::ToRemove:
               if(flags & Package::ToPurge) {
                   instructionList.insert(package->name(), Package::ToPurge);
               } else {
                   qDebug() << "Removing:" << package->name();
                   instructionList.insert(package->name(), Package::ToRemove);
               }
               break;
        }
    }

    qDebug() << instructionList;

    d->worker->commitChanges(instructionList);
}

void Backend::packageChanged(Package *package)
{
    qDebug() << "A package changed!";
    emit packageChanged();
}

void Backend::updateCache()
{
    Q_D(Backend);

    d->worker->updateCache();
}

void Backend::workerStarted()
{
    Q_D(Backend);

    connect(d->watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    connect(d->worker, SIGNAL(downloadProgress(int, int, int)),
            this, SIGNAL(downloadProgress(int, int, int)));
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
    disconnect(d->worker, SIGNAL(downloadMessage(int, const QString&)),
               this, SIGNAL(downloadMessage(int, const QString&)));
    disconnect(d->worker, SIGNAL(commitProgress(const QString&, int)),
               this, SIGNAL(commitProgress(const QString&, int)));
    disconnect(d->worker, SIGNAL(questionOccurred(int, const QVariantMap&)),
               this, SLOT(emitWorkerQuestionOccurred(int, const QVariantMap&)));
    disconnect(d->worker, SIGNAL(warningOccurred(int, const QVariantMap&)),
               this, SLOT(emitWarningOccurred(int, const QVariantMap&)));

    if (result) {
        reloadCache();
    }
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
    qDebug() << "Got a question";
}

void Backend::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    if (oldOwner.isEmpty()) {
        return; // Don't care, just appearing
    }

    if (newOwner.isEmpty()) {
        qDebug() << "It looks like our worker got lost";

        // Ok, something got screwed. Report and flee
        emit errorOccurred(QApt::WorkerDisappeared, QVariantMap());
        workerFinished(false);
    }
}

}
