/***************************************************************************
 *   Copyright © 2012 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *   Copyright © 2008-2009 Sebastian Heinlein <devel@glatzor.de>           *
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

#include "transactionqueue.h"

// Qt includes
#include <QtCore/QStringList>
#include <QtCore/QTimer>

// Own includes
#include "aptworker.h"
#include "transaction.h"

TransactionQueue::TransactionQueue(QObject *parent, AptWorker *worker)
    : QObject(parent)
    , m_worker(worker)
    , m_activeTransaction(nullptr)
{
}

QList<Transaction *> TransactionQueue::transactions() const
{
    return m_queue;
}

Transaction *TransactionQueue::activeTransaction() const
{
    return m_activeTransaction;
}

Transaction *TransactionQueue::transactionById(const QString &id)
{
    Transaction *transaction = nullptr;

    for (Transaction *trans : m_pending) {
        if (trans->transactionId() == id) {
            transaction = trans;
            break;
        }
    }

    return transaction;
}

void TransactionQueue::addPending(Transaction *trans)
{
    m_pending.append(trans);
}

void TransactionQueue::enqueue(QString tid)
{
    Transaction *trans = transactionById(tid);

    if (!trans)
        return;

    connect(trans, SIGNAL(finished(int)), this, SLOT(onTransactionFinished(int)));
    m_queue.enqueue(trans);
    emitQueueChanged();

    // Check if worker is running trans
    // If true, set trans status to wating
    // else, run trans
}

void TransactionQueue::remove(QString tid)
{
    Transaction *trans = transactionById(tid);

    if (!trans)
        return;

    m_queue.removeAll(trans);

    if (trans == m_activeTransaction)
        m_activeTransaction = nullptr;

    emitQueueChanged();

    // Wait in case clients are a bit slow.
    QTimer::singleShot(5000, trans, SLOT(deleteLater()));
}

void TransactionQueue::onTransactionFinished(int exitCode)
{
    Q_UNUSED(exitCode);

    Transaction *trans = qobject_cast<Transaction *>(sender());

    if (!trans) // Don't want no trouble...
        return;

    // TODO: Transaction chaining

    remove(trans->transactionId());
    runNextTransaction();
    emitQueueChanged();
}

void TransactionQueue::runNextTransaction()
{
    m_activeTransaction = m_queue.head();

    QMetaObject::invokeMethod(m_worker, "runTransaction", Qt::QueuedConnection,
                              Q_ARG(Transaction *, m_activeTransaction));
}

void TransactionQueue::emitQueueChanged()
{
    QString tid;
    QStringList queued;

    if (m_activeTransaction)
        tid = m_activeTransaction->transactionId();

    for (Transaction *trans : m_queue)
        queued << trans->transactionId();

    emit queueChanged(tid, queued);
}
