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

// Qt includes
#include <QObject>

// Apt-pkg includes
#include <apt-pkg/acquire.h>

class Transaction;

class WorkerAcquire : public QObject, public pkgAcquireStatus
{
    Q_OBJECT
public:
    explicit WorkerAcquire(QObject *parent, int begin = 0, int end = 100);

    void Start();
    void IMSHit(pkgAcquire::ItemDesc &Itm);
    void Fetch(pkgAcquire::ItemDesc &Itm);
    void Done(pkgAcquire::ItemDesc &Itm);
    void Fail(pkgAcquire::ItemDesc &Itm);
    void Stop();
    bool MediaChange(std::string Media, std::string Drive);

    bool Pulse(pkgAcquire *Owner);

    void setTransaction(Transaction *trans);

private:
    Transaction *m_trans;
    bool m_calculatingSpeed;
    int m_progressBegin;
    int m_progressEnd;
    int m_lastProgress;

private Q_SLOTS:
    void updateStatus(const pkgAcquire::ItemDesc &Itm);
};

#endif
