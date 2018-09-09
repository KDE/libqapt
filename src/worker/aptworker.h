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

#ifndef APTWORKER_H
#define APTWORKER_H

#include <QMutex>
#include <QProcess>
#include <QVector>

class QProcess;

class pkgCacheFile;
class pkgRecords;

class AptLock;
class Transaction;

class AptWorker : public QObject
{
    Q_OBJECT
public:
    explicit AptWorker(QObject *parent = 0);
    ~AptWorker();

    Transaction *currentTransaction();
    quint64 lastActiveTimestamp();

private:
    pkgCacheFile *m_cache;
    pkgRecords *m_records;
    QMutex m_transMutex;
    Transaction *m_trans;
    bool m_ready;
    QVector<AptLock *> m_locks;
    QMutex m_timestampMutex;
    quint64 m_lastActiveTimestamp;
    QProcess *m_dpkgProcess;

    /**
     * If the locks on the package system cannot be immediately taken, this
     * function will wait until the package system is unlocked, and proceed
     * to lock it.
     */
    void waitForLocks();

    /**
     * Releases APT locks and sets the transaction as done.
     */
    void cleanupCurrentTransaction();

    /**
     * Builds the package cache and package records.
     */
    void openCache(int begin = 0, int end = 5);

    /**
     * Checks for and downloads new package source lists.
     */
    void updateCache();

    /**
     * Marks changes as definied by the current transaction
     */
    bool markChanges();

    /**
     * Runs an APT commit
     */
    void commitChanges();

    /**
     * Upgrades packages
     */
    void upgradeSystem();

    /**
     * Installs a Debian package file by calling dpkg
     */
    void installFile();

    /**
     * Special function to download archives for DownloadArchivesRole transactions.
     */
    void downloadArchives();
    
public slots:
    /**
     * Initializes the worker's package system. This is done lazily to allow
     * the object to first be put in to another thread.
     */
    void init();

    /**
     * This function will run the provided transaction in a blocking fashion
     * until the transaction is complete. As such, it is suggested that this
     * class be run in a thread separate from ones e.g. looking for D-Bus
     * messages.
     *
     * @param trans
     */
    void runTransaction(Transaction *trans);

    /**
     * Stops the separate thread that AptWorker lives in. Call this before
     * exit to prevent the thread from whining that it was destroyed on shutdown.
     */
    void quit();

private slots:
    void dpkgStarted();
    void updateDpkgProgress();
    void dpkgFinished(int exitCode, QProcess::ExitStatus exitStatus);
};

#endif // APTWORKER_H
