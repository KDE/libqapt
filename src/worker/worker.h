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

#ifndef QAPTWORKER_H
#define QAPTWORKER_H

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtDBus/QDBusContext>

class pkgPolicy;
class pkgRecords;

class QEventLoop;
class QProcess;
class QTimer;

namespace QApt {
    class Cache;
}

class WorkerAcquire;
class WorkerInstallProgress;

class QAptWorker : public QCoreApplication, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kubuntu.qaptworker")
public:
    QAptWorker(int &argc, char **argv);

    virtual ~QAptWorker();

private:
    pid_t m_child_pid;

    QApt::Cache *m_cache;
    pkgPolicy *m_policy;
    pkgRecords *m_records;
    bool m_systemLocked;
    bool m_initialized;

    QVariantMap m_questionResponse;
    WorkerAcquire *m_acquireStatus;
    QEventLoop *m_questionBlock;
    QProcess *m_xapianProc;
    QProcess *m_dpkgProcess;
    QTimer *m_timeout;

public Q_SLOTS:
    void setLocale(const QString &locale) const;
    void setProxy(const QString &proxy) const;
    bool lockSystem();
    bool unlockSystem();
    void updateCache();
    void cancelDownload();
    void commitChanges(QVariantMap instructionsList);
    void downloadArchives(const QStringList &packages, const QString &destDir);
    void installDebFile(const QString &fileName);
    void answerWorkerQuestion(const QVariantMap &response);
    void updateXapianIndex();
    bool writeFileToDisk(const QString &contents, const QString &path);
    bool copyArchiveToCache(const QString &archivePath);

private Q_SLOTS:
    bool initializeApt();
    void reloadCache();
    void throwInitError();
    void initializeStatusWatcher();
    void dpkgStarted();
    void updateDpkgProgress();
    void dpkgFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void setAnswer(const QVariantMap &answer);
    void xapianUpdateFinished(bool result);

Q_SIGNALS:
    void workerStarted();
    void workerEvent(int code);
    // TODO QApt2: the result parameter is unused, remove it
    void workerFinished(bool result);
    // TODO QApt2: Rename globalDownloadProgress
    void downloadProgress(int percentage, int speed, int ETA);
    void packageDownloadProgress(const QString &name, int percentage, const QString &URI,
                                 double size, int flag);
    QT_DEPRECATED void downloadMessage(int flag, const QString &message);
    void commitProgress(const QString status, int percentage);
    void debInstallMessage(const QString &message);
    void errorOccurred(int code, const QVariantMap &details);
    void warningOccurred(int code, const QVariantMap &details);
    void questionOccurred(int questionCode, const QVariantMap& details);
    void answerReady(const QVariantMap& response);
    void xapianUpdateProgress(int percentage);
};

#endif
