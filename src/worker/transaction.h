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

#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QtCore/QObject>
#include <QtCore/QQueue>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusVariant>

#include "globals.h"

class Transaction : public QObject, protected QDBusContext
{
    Q_OBJECT

    Q_CLASSINFO("D-Bus Interface", "org.kubuntu.qaptworker.transaction")
    Q_PROPERTY(QString transactionId READ transactionId)
    Q_PROPERTY(int userId READ userId CONSTANT)
    Q_PROPERTY(int role READ role)
    Q_PROPERTY(int status READ status)
    Q_PROPERTY(QString locale READ locale)
    Q_PROPERTY(QString proxy READ proxy)
    Q_PROPERTY(QString debconfPipe READ debconfPipe)
public:
    Transaction(QObject *parent, QQueue<Transaction *> *queue, int userId);
    Transaction(QObject *parent, QApt::TransactionRole role,
                QQueue<Transaction *> *queue, int userId);

    QString transactionId() const;
    int userId() const;
    int role() const;
    int status() const;
    QString locale() const;
    QString proxy() const;
    QString debconfPipe() const;

    void setStatus(int status);

private:
    // Pointers to external containers
    QQueue<Transaction *> *m_queue;

    // Data members
    QString m_tid;
    int m_uid;
    QApt::TransactionRole m_role;
    QApt::TransactionStatus m_status;
    QString m_locale;
    QString m_proxy;
    QString m_debconfPipe;

    // Private functions
    int dbusSenderUid() const;
    bool isForeignUser() const;

Q_SIGNALS:
    Q_SCRIPTABLE void propertyChanged(int role, QDBusVariant newValue);
    
public Q_SLOTS:
    void setProperty(int property, QDBusVariant value);
    void run();

private Q_SLOTS:
    void setRole(int role);
    void setLocale(QString locale);
    void setProxy(QString proxy);
    void setDebconfPipe(QString pipe);
};

#endif // TRANSACTION_H
