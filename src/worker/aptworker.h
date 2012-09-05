/***************************************************************************
 *   Copyright © 20120-2012 Jonathan Thomas <echidnaman@kubuntu.org>       *
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

#include <QtCore/QObject>

class pkgRecords;

namespace QApt {
    class Cache;
}

class Transaction;

class AptWorker : public QObject
{
    Q_OBJECT
public:
    explicit AptWorker(QObject *parent = 0);
    ~AptWorker();

private:
    QApt::Cache *m_cache;
    pkgRecords *m_records;
    Transaction *m_trans;
    bool m_ready;

    void waitForLock();
    void openCache();

signals:
    
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

private slots:
};

#endif // APTWORKER_H
