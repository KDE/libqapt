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
#include <QtDBus/QDBusContext>

#include "../cache.h"

class pkgPolicy;
class pkgRecords;

class QEventLoop;

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

    bool m_locked;
    QVariantMap m_questionResponse;
    WorkerAcquire *m_acquireStatus;
    QEventLoop *m_questionBlock;

public Q_SLOTS:
    void updateCache();
    void cancelDownload();
    void commitChanges(QMap<QString, QVariant>);
    void answerWorkerQuestion(const QVariantMap& response);

private Q_SLOTS:
    bool lock();
    void unlock();
    bool initializeApt();
    void setAnswer(const QVariantMap &answer);
    void emitDownloadProgress(int percentage, int speed, int ETA);
    void emitDownloadMessage(int flag, const QString &message);
    void emitCommitProgress(const QString& status, int percentage);
    void emitErrorOccurred(int errorCode, const QVariantMap& details);
    void emitQuestionOccurred(int questionCode, const QVariantMap& details);

Q_SIGNALS:
    void workerStarted();
    void workerEvent(int code);
    void workerFinished(bool result);
    void downloadProgress(int percentage, int speed, int ETA);
    void downloadMessage(int flag, const QString &message);
    void commitProgress(const QString status, int percentage);
    void errorOccurred(int code, const QVariantMap &details);
    void warningOccurred(int code, const QVariantMap &details);
    void questionOccurred(int questionCode, const QVariantMap& details);
    void answerReady(const QVariantMap& response);
};

#endif
