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
#include <QtDBus/QDBusVariant>

#include "globals.h"

class Transaction : public QObject
{
    Q_OBJECT

    Q_CLASSINFO("D-Bus Interface", "org.kubuntu.qaptworker.transaction")
    Q_PROPERTY(QString transactionId READ transactionId)
    Q_PROPERTY(int role READ role)
public:
    explicit Transaction(QObject *parent = 0);

    QString transactionId() const;
    int role() const;

private:
    QString m_tid;
    QApt::TransactionRole m_role;

Q_SIGNALS:
    Q_SCRIPTABLE void propertyChanged(int role, QDBusVariant newValue);
    
public Q_SLOTS:
    bool setRole(int role);
};

#endif // TRANSACTION_H
