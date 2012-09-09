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

#ifndef TRANSACTION_P_H
#define TRANSACTION_P_H

// Qt includes
#include <QtDBus/QDBusPendingCallWatcher>
#include <QDebug>

// Own includes
#include "globals.h"
#include "transactiondbus.h"

namespace QApt {

class TransactionPrivate : public QObject
{
    Q_OBJECT
    public:
        TransactionPrivate(const QString &id)
            : QObject(0)
            , tid(id)
        {
            dbus = new OrgKubuntuQaptworkerTransactionInterface(QLatin1String("org.kubuntu.qaptworker"),
                                                                       tid, QDBusConnection::systemBus(),
                                                                       0);
        }

        TransactionPrivate(const TransactionPrivate &other)
            : QObject(0)
            , dbus(other.dbus)
            , tid(other.tid)
            , uid(0)
            , role(EmptyRole)
            , status(QApt::SetupStatus)
            , error(QApt::Success)
            , isCancellable(true)
            , isCancelled(false)
            , exitStatus(QApt::ExitUnfinished)
            , isPaused(false)
            , progress(0)
        {
        }

        ~TransactionPrivate()
        {
            delete dbus;
        }

        // DBus
        OrgKubuntuQaptworkerTransactionInterface *dbus;

        // Data
        QString tid;
        int uid;
        TransactionRole role;
        TransactionStatus status;
        ErrorCode error;
        QString locale;
        QString proxy;
        QString debconfPipe;
        QVariantMap packages;
        bool isCancellable;
        bool isCancelled;
        ExitStatus exitStatus;
        bool isPaused;
        QString statusDetails;
        int progress;

        void run();

    private Q_SLOTS:
        void onRunCallFinished(QDBusPendingCallWatcher *watcher);
};

}

#endif // TRANSACTION_P_H
