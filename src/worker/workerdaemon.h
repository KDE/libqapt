/***************************************************************************
 *   Copyright © 2012 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#ifndef WORKERDAEMON_H
#define WORKERDAEMON_H

#include <QCoreApplication>
#include <QDBusContext>

#include "globals.h"

class QThread;
class QTimer;

class AptWorker;
class Transaction;
class TransactionQueue;

class WorkerDaemon : public QCoreApplication, protected QDBusContext
{
    Q_OBJECT

    Q_CLASSINFO("D-Bus Interface", "org.kubuntu.qaptworker")
public:
    WorkerDaemon(int &argc, char **argv);

private:
    TransactionQueue *m_queue;
    AptWorker *m_worker;
    QThread *m_workerThread;
    QTimer *m_idleTimer;

    int dbusSenderUid() const;
    Transaction *createTransaction(QApt::TransactionRole role,
                                   QVariantMap instructionsList = QVariantMap());

signals:
    Q_SCRIPTABLE void transactionQueueChanged(const QString &active,
                                              const QStringList &queued);
    
public slots:
    // Transaction-based methods. Return transaction ids.
    QString updateCache();
    QString installFile(const QString &file);
    QString commitChanges(QVariantMap instructionsList);
    QString upgradeSystem(bool safeUpgrade);
    QString downloadArchives(const QStringList &packageNames, const QString &dest);

    // Synchronous methods
    bool writeFileToDisk(const QString &contents, const QString &path);
    bool copyArchiveToCache(const QString &archivePath);

private slots:
    void checkIdle();
};

#endif // WORKERDAEMON_H
