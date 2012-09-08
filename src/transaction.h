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

#ifndef QAPT_TRANSACTION_H
#define QAPT_TRANSACTION_H

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

/**
 * The QApt namespace is the main namespace for LibQApt. All classes in this
 * library fall under this namespace.
 */
namespace QApt {

class TransactionPrivate;

/**
 * The Transaction class is an interface for a Transaction in the QApt Worker.
 * Transactions represent asynchronous tasks being performed by the QApt Worker,
 * such as committing packages, or checking for updates. This class provides
 * information about transactions, as well as means to interact with them on the
 * client side.
 *
 * @author Jonathan Thomas
 * @since 2.0
 */
class Q_DECL_EXPORT Transaction : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString transactionId READ transactionId)
    Q_PROPERTY(int userId READ userId WRITE setUserId)
public:
    /**
     * Constructor
     *
     * @param parent The parent of the object
     * @param tid The D-Bus path (transaction id) of the transaction
     */
    Transaction(const QString &tid);

    Transaction(const Transaction &other);

    /**
     * Destructor
     */
    ~Transaction();

    Transaction &operator=(const Transaction& rhs);

    QString transactionId() const;
    int userId() const;

private:
    QSharedPointer<TransactionPrivate> d;

    void setUserId(int id);

Q_SIGNALS:

public Q_SLOTS:

private Q_SLOTS:
    void sync();
};

}

#endif // QAPT_TRANSACTION_H
