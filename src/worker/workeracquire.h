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

#ifndef WORKERACQUIRE_H
#define WORKERACQUIRE_H

#include <QtCore/QVariantMap>

#include <apt-pkg/acquire.h>

#include "globals.h"

class Transaction;
class QAptWorker;

class WorkerAcquire : public QObject, public pkgAcquireStatus
{
    Q_OBJECT
    Q_ENUMS(FetchType)
public:
    explicit WorkerAcquire(QAptWorker *parent, int begin = 0, int end = 100);

    void Start();
    void IMSHit(pkgAcquire::ItemDesc &Itm);
    void Fetch(pkgAcquire::ItemDesc &Itm);
    void Done(pkgAcquire::ItemDesc &Itm);
    void Fail(pkgAcquire::ItemDesc &Itm);
    void Stop();
    bool MediaChange(string Media, string Drive);

    bool Pulse(pkgAcquire *Owner);

    bool wasCancelled() const;
    void setTransaction(Transaction *trans);

private:
    QVariantMap askQuestion(int questionCode, const QVariantMap &args);

    Transaction *m_trans;
    bool m_calculatingSpeed;
    int m_progressBegin;
    int m_progressEnd;
    int m_lastProgress;

public Q_SLOTS:
    void requestCancel();

private Q_SLOTS:
    void setAnswer(const QVariantMap &response);
    void updateStatus(const pkgAcquire::ItemDesc &Itm, int percentage, int status);

signals:
    void fetchError(int errorCode, const QVariantMap &details);
    void fetchWarning(int warningCode, const QVariantMap &details);
    void workerQuestion(int questionCode, const QVariantMap &args);
    void downloadProgress(int percentage, int speed, int ETA);
    void packageDownloadProgress(const QString &name, int percentage, const QString &URI,
                                 double size, int flag);
};

#endif
