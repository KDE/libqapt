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

#include "worker.h"

#include "qaptworkeradaptor.h"

// QApt includes
#include "../cache.h"
#include "../globals.h"
#include "../package.h"

// Apt includes
#include <apt-pkg/algorithms.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>

#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

// Qt includes
#include <QtCore/QProcess>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#define RAMFS_MAGIC     0x858458f6

#include "qaptauthorization.h"
#include "workeracquire.h"
#include "workerinstallprogress.h"

QAptWorker::QAptWorker(int &argc, char **argv)
        : QCoreApplication(argc, argv)
        , m_cache(0)
        , m_records(0)
        , m_questionBlock(0)
        , m_questionResponse(QVariantMap())
{
    new QaptworkerAdaptor(this);

    if (!QDBusConnection::systemBus().registerService("org.kubuntu.qaptworker")) {
        // Another worker is already here, quit
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        return;
    }

    if (!QDBusConnection::systemBus().registerObject("/", this)) {
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        return;
    }

    QTimer::singleShot(10000, this, SLOT(quit()));
    sleep(1);

    m_acquireStatus = new WorkerAcquire(this);
    connect(m_acquireStatus, SIGNAL(globalDownloadProgress(int, int, int)),
            this, SIGNAL(globalDownloadProgress(int, int, int)));
    connect(m_acquireStatus, SIGNAL(packageDownloadProgress(const QString&, int)),
            this, SIGNAL(packageDownloadProgress(const QString&, int)));
    connect(m_acquireStatus, SIGNAL(downloadMessage(int, const QString&)),
            this, SIGNAL(downloadMessage(int, const QString&)));
    connect(m_acquireStatus, SIGNAL(fetchError(int, const QVariantMap&)),
            this, SIGNAL(errorOccurred(int, const QVariantMap&)));
    connect(m_acquireStatus, SIGNAL(fetchWarning(int, const QVariantMap&)),
            this, SIGNAL(warningOccurred(int, const QVariantMap&)));
    connect(m_acquireStatus, SIGNAL(workerQuestion(int, const QVariantMap&)),
            this, SIGNAL(questionOccurred(int, const QVariantMap&)));
}

QAptWorker::~QAptWorker()
{
    delete m_cache;
    delete m_records;
}

void QAptWorker::setLocale(const QString &locale) const
{
    std::setlocale(LC_ALL, locale.toAscii());
}

bool QAptWorker::initializeApt()
{
    if (!pkgInitConfig(*_config)) {
        return false;
    }

    if (!pkgInitSystem(*_config, _system)) {
        return false;
    }

    delete m_cache;
    m_cache = new QApt::Cache(this);
    if (!m_cache->open()) {
        QVariantMap details;
        string message;
        bool isError = _error->PopMessage(message);
        if (isError) {
            details["ErrorText"] = QString::fromStdString(message);
        }
        emit errorOccurred(QApt::InitError, details);
        return false;
    }

    pkgDepCache *depCache = m_cache->depCache();

    delete m_records;
    m_records = new pkgRecords(*depCache);

    return true;
}

void QAptWorker::updateCache()
{
    if (!QApt::Auth::authorize("org.kubuntu.qaptworker.updateCache", message().service())) {
        emit errorOccurred(QApt::AuthError, QVariantMap());
        return;
    }

    emit workerStarted();
    emit workerEvent(QApt::CacheUpdateStarted);

    if (!initializeApt()) {
        emit workerFinished(false);
        return;
    }
    // Lock the list directory
    FileFd Lock;
    if (!_config->FindB("Debug::NoLocking", false)) {
        Lock.Fd(GetLock(_config->FindDir("Dir::State::Lists") + "lock"));
        if (_error->PendingError()) {
            emit errorOccurred(QApt::LockError, QVariantMap());
            emit workerFinished(false);
            return;
        }
    }

    pkgAcquire fetcher(m_acquireStatus);
    // Populate it with the source selection and get all Indexes
    // (GetAll=true)
    if (m_cache->list()->GetIndexes(&fetcher,true) == false) {
        emit workerFinished(false);
        return;
    }

    // do the work
    if (_config->FindB("APT::Get::Download",true) == true) {
        bool result = ListUpdate(*m_acquireStatus, *m_cache->list());
        emit workerFinished(result);
    } else {
        emit errorOccurred(QApt::DownloadDisallowedError, QVariantMap());
        emit workerFinished(false);
    }

    emit workerEvent(QApt::CacheUpdateFinished);
}

void QAptWorker::cancelDownload()
{
    m_acquireStatus->requestCancel();
}

void QAptWorker::commitChanges(QMap<QString, QVariant> instructionsList)
{
    if (!QApt::Auth::authorize("org.kubuntu.qaptworker.commitChanges", message().service())) {
        emit errorOccurred(QApt::AuthError, QVariantMap());
        return;
    }

    if (!initializeApt()) {
        emit workerFinished(false);
        return;
    }

    QVariantMap versionList;

    // Parse out the argument list and mark packages for operations
    QVariantMap::const_iterator mapIter = instructionsList.constBegin();

    while (mapIter != instructionsList.constEnd()) {
        int operation = mapIter.value().toInt();

        // Find package in cache
        pkgCache::PkgIterator iter;
        QString packageString = mapIter.key();
        QString version;

        if (packageString.contains(',')) {
            QStringList split = packageString.split(',');
            iter = m_cache->depCache()->FindPkg(split.at(0).toStdString());
            version = split.at(1);
        } else {
            iter = m_cache->depCache()->FindPkg(packageString.toStdString());
        }
        if (iter == 0) {
            QVariantMap args;
            args["NotFoundString"] = packageString;
            emit errorOccurred(QApt::NotFoundError, args);
            emit workerFinished(false);
            return;
        }

        pkgDepCache::StateCache & State = (*m_cache->depCache())[iter];
        pkgProblemResolver Fix(m_cache->depCache());

        // Then mark according to the instruction
        switch (operation) {
            case QApt::Package::Held:
                m_cache->depCache()->MarkKeep(iter, false);
                m_cache->depCache()->SetReInstall(iter, false);
                break;
            case QApt::Package::ToUpgrade:
            case QApt::Package::ToInstall:
                m_cache->depCache()->MarkInstall(iter, true);
                if (!State.Install() || m_cache->depCache()->BrokenCount() > 0) {
                    pkgProblemResolver Fix(m_cache->depCache());
                    Fix.Clear(iter);
                    Fix.Protect(iter);
                    Fix.Resolve(true);
                }
                break;
            case QApt::Package::ToReInstall:
                m_cache->depCache()->SetReInstall(iter, true);
                break;
            case QApt::Package::ToDowngrade: {
                pkgVersionMatch Match(version.toStdString(), pkgVersionMatch::Version);
                pkgCache::VerIterator Ver = Match.Find(iter);

                m_cache->depCache()->SetCandidateVersion(Ver);

                m_cache->depCache()->MarkInstall(iter, true);
                if (!State.Install() || m_cache->depCache()->BrokenCount() > 0) {
                    pkgProblemResolver Fix(m_cache->depCache());
                    Fix.Clear(iter);
                    Fix.Protect(iter);
                    Fix.Resolve(true);
                }
                break;
            }
            case QApt::Package::ToPurge:
                m_cache->depCache()->SetReInstall(iter, false);
                m_cache->depCache()->MarkDelete(iter, true);
                break;
            case QApt::Package::ToRemove:
                m_cache->depCache()->SetReInstall(iter, false);
                m_cache->depCache()->MarkDelete(iter, false);
                break;
            default:
                // Unsupported operation. Should emit something here?
                break;
        }
        mapIter++;
    }

    emit workerStarted();

    // Lock the archive directory
    FileFd Lock;
    if (_config->FindB("Debug::NoLocking",false) == false)
    {
        Lock.Fd(GetLock(_config->FindDir("Dir::Cache::Archives") + "lock"));
        if (_error->PendingError()) {
            emit errorOccurred(QApt::LockError, QVariantMap());
            emit workerFinished(false);
            return;
        }
    }

    pkgAcquire fetcher(m_acquireStatus);

    pkgPackageManager *packageManager;
    packageManager = _system->CreatePM(m_cache->depCache());

    if (!packageManager->GetArchives(&fetcher, m_cache->list(), m_records) ||
        _error->PendingError()) {
        emit errorOccurred(QApt::FetchError, QVariantMap());
        emit workerFinished(false);
        return;
    }

    // Display statistics
    double FetchBytes = fetcher.FetchNeeded();
    double FetchPBytes = fetcher.PartialPresent();
    double DebBytes = fetcher.TotalNeeded();
    if (DebBytes != m_cache->depCache()->DebSize())
    {
        emit warningOccurred(QApt::SizeMismatchWarning, QVariantMap());
    }

    /* Check for enough free space */
    struct statvfs Buf;
    string OutputDir = _config->FindDir("Dir::Cache::Archives");
    if (statvfs(OutputDir.c_str(),&Buf) != 0) {
        QVariantMap args;
        args["DirectoryString"] = QString::fromStdString(OutputDir.c_str());
        emit errorOccurred(QApt::DiskSpaceError, args);
        emit workerFinished(false);
        return;
    }
    if (unsigned(Buf.f_bfree) < (FetchBytes - FetchPBytes)/Buf.f_bsize)
    {
        struct statfs Stat;
        if (statfs(OutputDir.c_str(), &Stat) != 0 ||
            unsigned(Stat.f_type)            != RAMFS_MAGIC)
        {
            QVariantMap args;
            args["DirectoryString"] = QString::fromStdString(OutputDir.c_str());
            emit errorOccurred(QApt::DiskSpaceError, args);
            emit workerFinished(false);
            return;
        }
    }

    QStringList untrustedPackages;
    for (pkgAcquire::ItemIterator I = fetcher.ItemsBegin(); I < fetcher.ItemsEnd(); ++I)
    {
        if (!(*I)->IsTrusted())
        {
            untrustedPackages << QString::fromStdString((*I)->ShortDesc());
        }
    }

    if (!untrustedPackages.isEmpty()) {
        QVariantMap args;
        args["UntrustedItems"] = untrustedPackages;

        if (_config->FindB("APT::Get::AllowUnauthenticated", true) == true) {
            // Ask if the user really wants to install untrusted packages, if
            // allowed in the APT config.
            m_questionBlock = new QEventLoop;
            connect(this, SIGNAL(answerReady(const QVariantMap&)),
                    this, SLOT(setAnswer(const QVariantMap&)));

            emit questionOccurred(QApt::InstallUntrusted, args);
            m_questionBlock->exec();

            bool m_installUntrusted = m_questionResponse["InstallUntrusted"].toBool();
            if(!m_installUntrusted) {
                m_questionResponse = QVariantMap(); //Reset for next question
                emit workerFinished(false);
                return;
            } else {
                m_questionResponse = QVariantMap(); //Reset for next question
            }
        } else {
            // If disallowed in APT config, return a fatal error
            emit errorOccurred(QApt::UntrustedError, args);
            emit workerFinished(false);
            return;
        }
    }

    emit workerEvent(QApt::PackageDownloadStarted);

    if (fetcher.Run() != pkgAcquire::Continue) {
        // Our fetcher will report errors for itself, but we have to send the
        // finished signal
        emit workerFinished(false);
        return;
    }

    emit workerEvent(QApt::PackageDownloadFinished);

    emit workerEvent(QApt::CommitChangesStarted);

    WorkerInstallProgress *installProgress = new WorkerInstallProgress(this);
    connect(installProgress, SIGNAL(commitError(int, const QVariantMap&)),
            this, SIGNAL(errorOccurred(int, const QVariantMap&)));
    connect(installProgress, SIGNAL(commitProgress(const QString&, int)),
            this, SIGNAL(commitProgress(const QString&, int)));
    connect(installProgress, SIGNAL(workerQuestion(int, const QVariantMap&)),
            this, SIGNAL(questionOccurred(int, const QVariantMap&)));

    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

    pkgPackageManager::OrderResult res = installProgress->start(packageManager);
    bool success = (res == pkgPackageManager::Completed);

    if (success) {
        emit workerEvent(QApt::CommitChangesFinished);
    }
    emit workerFinished(success);

    delete installProgress;
    installProgress = 0;
}

void QAptWorker::answerWorkerQuestion(const QVariantMap &response)
{
    emit answerReady(response);
}

void QAptWorker::setAnswer(const QVariantMap &answer)
{
    disconnect(this, SIGNAL(answerReady(const QVariantMap&)),
               this, SLOT(setAnswer(const QVariantMap&)));
    m_questionResponse = answer;
    m_questionBlock->quit();
}

void QAptWorker::updateXapianIndex()
{
    emit workerEvent(QApt::XapianUpdateStarted);
    m_xapianProc = new QProcess(this);
    QString cmd = "/usr/bin/nice /usr/bin/ionice -c3 "
                  "/usr/sbin/update-apt-xapian-index "
                  "--update";
    m_xapianProc->start(cmd);

    m_xapianProc->waitForFinished();
    emit workerEvent(QApt::XapianUpdateFinished);
}
