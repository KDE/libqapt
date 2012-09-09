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
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusVariant>

#include "globals.h"

class QDBusPendingCallWatcher;

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

    Q_PROPERTY(QString transactionId READ transactionId WRITE updateTransactionId)
    Q_PROPERTY(int userId READ userId WRITE updateUserId)
    Q_PROPERTY(TransactionRole role READ role WRITE updateRole)
    Q_PROPERTY(TransactionStatus status READ status WRITE updateStatus)
    Q_PROPERTY(ErrorCode error READ error)
    Q_PROPERTY(QString locale READ locale WRITE updateLocale)
    Q_PROPERTY(QString proxy READ proxy WRITE updateProxy)
    Q_PROPERTY(QString debconfPipe READ debconfPipe WRITE updateDebconfPipe)
    Q_PROPERTY(QVariantMap packages READ packages WRITE updatePackages)
    Q_PROPERTY(bool isCancellable READ isCancellable WRITE updateCancellable)
    Q_PROPERTY(bool isCancelled READ isCancelled WRITE updateCancelled)
    Q_PROPERTY(ExitStatus exitStatus READ exitStatus WRITE updateExitStatus)
    Q_PROPERTY(bool isPaused READ isPaused WRITE updatePaused)
    Q_PROPERTY(QString statusDetails READ statusDetails WRITE updateStatusDetails)
    Q_PROPERTY(int progress READ progress WRITE updateProgress)

    Q_ENUMS(TransactionRole)
    Q_ENUMS(TransactionStatus)
    Q_ENUMS(ErrorCode)
    Q_ENUMS(ExitStatus)
public:
    /**
     * Constructor
     *
     * @param parent The parent of the object
     * @param tid The D-Bus path (transaction id) of the transaction
     */
    Transaction(const QString &tid);

    /**
     * The copy constructor.
     *
     * This will create a new transaction instance representing the same worker
     * transaction as the one being copied. Delete it when you are done with it.
     *
     * @param other The transaction to be copied
     */
    Transaction(const Transaction &other);

    /**
     * Destructor
     */
    ~Transaction();

    /**
     * Assignment operator.
     *
     * This will create a new transaction instance representing the same worker
     * transaction as the one being copied. Delete it when you are done with it.
     *
     * @param other The transaction to be copied
     */
    Transaction &operator=(const Transaction& rhs);

    QString transactionId() const;
    int userId() const;
    QApt::TransactionRole role() const;
    QApt::TransactionStatus status() const;
    QApt::ErrorCode error() const;
    QString locale() const;
    QString proxy() const;
    QString debconfPipe() const;
    QVariantMap packages() const;
    bool isCancellable() const;
    bool isCancelled() const;
    QApt::ExitStatus exitStatus() const;
    bool isPaused() const;
    QString statusDetails() const;
    int progress() const;

private:
    QSharedPointer<TransactionPrivate> d;

    void updateTransactionId(const QString &tid);
    void updateUserId(int id);
    void updateRole(QApt::TransactionRole role);
    void updateStatus(QApt::TransactionStatus status);
    void updateError(QApt::ErrorCode);
    void updateLocale(const QString &locale);
    void updateProxy(const QString &proxy);
    void updateDebconfPipe(const QString &pipe);
    void updatePackages(const QVariantMap &packages);
    void updateCancellable(bool cancellable);
    void updateCancelled(bool cancelled);
    void updateExitStatus(QApt::ExitStatus exitStatus);
    void updatePaused(bool paused);
    void updateStatusDetails(const QString &details);
    void updateProgress(int progress);

Q_SIGNALS:
    void errorOccurred(QApt::ErrorCode error);
    void statusChanged(QApt::TransactionStatus status);
    void cancellableChanged(bool cancellable);
    void paused();
    void resumed();
    void statusDetailsChanged(const QString &statusDetails);
    void progressChanged(int progress);
    void finished(QApt::ExitStatus exitStatus);

public Q_SLOTS:
    void setLocale(const QString &locale);
    void setProxy(const QString &proxy);
    void run();

private Q_SLOTS:
    void sync();
    void updateProperty(int type, const QDBusVariant &variant);
    void onCallFinished(QDBusPendingCallWatcher *watcher);
    void emitFinished(int exitStatus);
};

}

#endif // QAPT_TRANSACTION_H
