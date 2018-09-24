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

#include <QObject>
#include <QVariantMap>

#include "downloadprogress.h"

class QDBusPendingCallWatcher;
class QDBusVariant;

#ifdef __CURRENTLY_UNIT_TESTING__
class TransactionErrorHandlingTest;
#endif

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

#ifdef __CURRENTLY_UNIT_TESTING__
    friend TransactionErrorHandlingTest;
#endif
    
    Q_ENUMS(TransactionRole)
    Q_ENUMS(TransactionStatus)
    Q_ENUMS(ErrorCode)
    Q_ENUMS(ExitStatus)

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
    Q_PROPERTY(DownloadProgress downloadProgress READ downloadProgress WRITE updateDownloadProgress)
    Q_PROPERTY(QStringList untrustedPackages READ untrustedPackages WRITE updateUntrustedPackages)
    Q_PROPERTY(quint64 downloadSpeed READ downloadSpeed WRITE updateDownloadSpeed)
    Q_PROPERTY(quint64 downloadETA READ downloadETA WRITE updateDownloadETA)
    Q_PROPERTY(QString filePath READ filePath WRITE updateFilePath)
    Q_PROPERTY(QString errorDetails READ errorDetails WRITE updateErrorDetails)
    Q_PROPERTY(FrontendCaps frontendCaps READ frontendCaps WRITE updateFrontendCaps)

public:
    /**
     * Constructor
     * @param tid The D-Bus path (transaction id) of the transaction
     */
    explicit Transaction(const QString &tid);

    /// Destructor
    ~Transaction();

    /**
     * Equality operator.
     *
     * This will compare the transaction IDs of the two Transaction objects to
     * determine whether or not the two transactions are "the same". They will
     * usually not point to the same address, but they still represent the same
     * transaction on D-Bus
     *
     * @param rhs The transaction to be compared to
     */
    bool operator==(const Transaction* rhs) const;

    /**
     * Returns a unique identifier for the transaction. This is useful for
     * interacting with certain QApt::Backend API, which refers to transactions
     * by their transaction ID when notifying of worker queue changes.
     */
    QString transactionId() const;

    /// Returns the user ID of the user that initiated the transaction.
    int userId() const;

    /**
     * Returns the role of the transaction, describing what action the transaction
     * will perform.
     */
    QApt::TransactionRole role() const;

    /// Returns the current status of the transaction
    QApt::TransactionStatus status() const;

    /**
     * Returns the error code set by the QApt Worker in the event the transaction
     * ended in error. Returns QApt::Success if no error has occurred at the time
     * the method is called.
     */
    QApt::ErrorCode error() const;
    
    /**
     * Returns the error string for the error code that may or may not be set.
     * Returns QString::null if no error
     */
    QString errorString() const;

    /**
     * Returns the locale code the transaction is using to perform translations
     * and encoding. Defaults to an empty string.
     *
     * @see setLocale
     */
    QString locale() const;

    /**
     * Returns the HTTP proxy used by the transaction to perform actions that
     * require fetching data over HTTP. Defaults to an empty string.
     *
     * @see setProxy
     */
    QString proxy() const;

    /**
     * Returns the Debconf socket used by the transaction to handle debconf
     * communication. Use this socket to set up your Debconf frontend on the
     * client side. Defaults to an empty string.
     *
     * @see setDebconfPipe
     */
    QString debconfPipe() const;

    /// Returns a list of the packages that this transaction will act upon.
    QVariantMap packages() const;

    /// Returns true if the transaction can be cancelled; otherwise, false.
    bool isCancellable() const;

    /// Returns true if the transaction has been cancelled; otherwise, false.
    bool isCancelled() const;

    /**
     * Returns the exit status of the transaction. If the transaction has not
     * yet finished, it will return QApt::ExitUnfinished
     */
    QApt::ExitStatus exitStatus() const;

    /**
     * Returns true if the transaction has been paused; otherwise, false.
     *
     * A transaction will be paused by the worker while waiting for user input.
     *
     * @see paused
     * @see resumed
     */
    bool isPaused() const;

    /**
     * Returns a human-readable status message providing further information
     * about the current transaction status. For example, during the
     * QApt::CommittingStatus, status messages from APT will be returned.
     *
     * If no details about the current status are available, an empty string
     * will be returned.
     *
     * @see statusDetailsChanged status
     */
    QString statusDetails() const;

    /**
     * Returns the current overall progress of the transaction as a percentage.
     *
     * If a number over 100 is returned, no progress can be determined, and
     * progress bars displaying this progress should be placed in "busy" or
     * "indeterminate" mode.
     *
     * @see progressChanged
     */
    int progress() const;

    /**
     * Returns a QApt::DownloadProgress object which describes the most recent
     * download progress information received from the worker.
     *
     * @see downloadProgressChanged
     */
    QApt::DownloadProgress downloadProgress() const;

    /**
     * Returns the list of untrusted packages that the transaction will potentially
     * act upon. The QApt Worker will set this list if the transaction will act
     * upon untrusted packages.
     *
     * @see promptUntrusted
     * @see replyUntrustedPrompt
     */
    QStringList untrustedPackages() const;

    /**
     * Returns the rate at which data is being downloaded by the transaction in bytes.
     *
     * @see downloadSpeedChanged
     */
    quint64 downloadSpeed() const;

    /**
     * Returns the estimated time until the completion of a transaction's
     * download state, in seconds.
     *
     * @see downloadETAChanged
     */
    quint64 downloadETA() const;

    /**
     * Returns the path of the file currently being installed for transactions
     * with a TransactionRole of InstallFileRole.
     *
     * @see setFilePath
     */
    QString filePath() const;

    /**
     * Returns a string containing the details describing a transaction error.
     *
     * If no errors have occurred, this will return an empty string.
     */
    QString errorDetails() const;

    /**
     * Returns the frontend capabilities of the frontend displaying the
     * transaction.
     */
    QApt::FrontendCaps frontendCaps() const;

private:
    TransactionPrivate *const d;

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
    void updateDownloadProgress(const QApt::DownloadProgress &downloadProgress);
    void updateUntrustedPackages(const QStringList &untrusted);
    void updateDownloadSpeed(quint64 downloadSpeed);
    void updateDownloadETA(quint64 ETA);
    void updateFilePath(const QString &filePath);
    void updateErrorDetails(const QString &errorDetails);
    void updateFrontendCaps(QApt::FrontendCaps frontendCaps);

Q_SIGNALS:
    /**
     * This signal is emitted when the transaction encounters a fatal error
     * that prevents the transaction from successfully finishing.
     *
     * Slots connected to this signal should notify the user that an error
     * has occurred.
     *
     * @see errorDetails
     */
    void errorOccurred(QApt::ErrorCode error);

    /**
     * This signal is emitted when the status of the transaction changes.
     *
     * Slots connected to this signal should use this to react to these
     * state changes, updating their interfaces and reloading the backend
     * as needed.
     */
    void statusChanged(QApt::TransactionStatus status);

    /**
     * This signal is emitted when the transaction enters a state where
     * it can or cannot be cancelled, depending on the parameter given.
     *
     * Slots connected to this signal should disable or re-enable cancel
     * buttons in their interface as appropriate.
     *
     * @param cancellable Whether or not the transaction is cancellable
     */
    void cancellableChanged(bool cancellable);

    /// This signal is emitted whenever the QApt worker pauses the transaction
    void paused();

    /// This signal is emitted whenever the QApt worker resumes the transaction
    void resumed();

    /**
     * This signal is emitted when the status details of the transaction change.
     *
     * @param statusDetails The latest status details
     * @see statusDetails
     */
    void statusDetailsChanged(const QString &statusDetails);

    /**
     * This signal is emitted when the global progress of the transaction changes.
     *
     *@param progress The new current progress, as a percentage.
     */
    void progressChanged(int progress);

    /**
     * This signal is emitted when new download progress information becomes
     * available.
     *
     * @param progress The latest download progress info
     */
    void downloadProgressChanged(QApt::DownloadProgress progress);

    /**
     * This signal is emitted when the transaction reaches the Finished state.
     *
     * Slots connected to this signal should perform post-transaction cleanup
     * and delete the transaction object once they are done retreiving data
     * from it.
     *
     * @param exitStatus The exit status of the transaction
     */
    void finished(QApt::ExitStatus exitStatus);

    /**
     * This signal is emitted when during the course of downloading packages,
     * the QApt Worker encounters packages that need to be downloaded from
     * a CD-ROM or DVD-ROM disc not currently mounted. The worker will pause
     * this transaction until a response is given by the provideMedium()
     * method.
     *
     * Slots connected to this signal should notify the user about this
     * requirement, prompting to insert the CD in the drive at the given
     * mount point.
     *
     * @param label The label of the required optical media
     * @param mountPoint The mount point of the optical medium drive
     * to place the media
     *
     * @see provideMedium
     */
    void mediumRequired(QString label, QString mountPoint);

    /**
     * This signal is emitted when the QApt Worker detects that packages will
     * need to be downloaded from an untrusted source to complete the
     * transaction. The worker will pause the transaction until a response is
     * given by the replyUntrustedPrompt method.
     *
     * @param untrustedPackages The list of untrusted packages
     *
     * @see replyUntrustedPrompt
     */
    void promptUntrusted(QStringList untrustedPackages);

    /**
     * This signal is emitted when a new version of a package being installed
     * contains an updated version of a configuration file, and the user has
     * made custom changes to the existing version of the configuration file.
     * The QApt Worker will pause the transaction until a response is given
     * by the resolveConfigFileConflict() method
     *
     * Slots connected to this signal should prompt the user to determine if
     * the worker should keep the current configuration, or install the new
     * configuration file. The @a currentPath argument indicates the location
     * of the existing configuration file, and the @a newPath argument is the
     * location of a copy of the new configuration file, which can be used to
     * display the difference between the two files, if desired.
     *
     * @param currentPath The path of the config file to be replaced
     * @param newPath The path to a copy of the new config file
     *
     * @see resolveConfigFileConflict
     */
    void configFileConflict(QString currentPath, QString newPath);

    /**
     * This signal is emitted when the current download rate changes.
     *
     * @param downloadSpeed The current download rate in bytes per second
     *
     * @see downloadSpeed
     */
    void downloadSpeedChanged(quint64 downloadSpeed);

    /**
     * This signal is emitted when the time estimate for a transaction's
     * download changes.
     *
     * @param ETA The estimated time until download completion in seconds
     *
     * @see downloadETA
     */
    void downloadETAChanged(quint64 ETA);

public Q_SLOTS:
    /**
     * Sets the locale code to be used by the transaction for translation and
     * encoding purposes.
     *
     * This property can only be changed before the transaction is run.
     *
     * @param locale The locale code to be used
     *
     * @see locale
     */
    void setLocale(const QString &locale);

    /**
     * Sets the HTTP proxy to be used by the transaction during actions that
     * require fetching data over HTTP.
     *
     * This property can only be changed before the transaction is run.
     *
     * @param proxy The HTTP proxy for the transaction to use
     *
     * @see proxy
     */
    void setProxy(const QString &proxy);

    /**
     * Sets the Debconf socket used by the transaction to handle debconf
     * communication. Use this socket to set up your Debconf frontend on the
     * client side.
     *
     * This property can only be changed before the transaction is run.
     *
     * @param pipe The debconf socket to be used by the worker
     *
     * @see debconfPipe
     */
    void setDebconfPipe(const QString &pipe);

    /**
     * Sets the frontend capabilities for the frontend handling this
     * transaction. E.g. advertise support for debconf, etc.
     */
    void setFrontendCaps(QApt::FrontendCaps frontendCaps);

    /**
     * Queues the transaction to be processed by the QApt Worker.
     */
    void run();

    /**
     * Attempts to cancel the transaction. The transaction will only be
     * cancelled if the @a isCancellable property is true.
     *
     * @see isCancellable
     */
    void cancel();

    /**
     * Responds to the QApt Worker's request for an installation medium.
     *
     * The mount point of the medium the worker requested must be provided
     * as a sanity check.
     *
     * @param medium The mount point of the medium the user has provided
     */
    void provideMedium(const QString &medium);

    /**
     * Responds to the QApt Worker's query of whether or not the user wishes
     * to continue with the transaction, despite the need to download
     * untrusted packages.
     *
     * @param approved Whether or not the user wishes to continue
     */
    void replyUntrustedPrompt(bool approved);

    /**
     * Responds to the QApt Worker's prompt of a configuration file conflict.
     *
     * The path of the configuration file being replaced must be provided as a
     * sanity check.
     *
     * @param currentPath The configuration file being replaced
     * @param replace Whether or not the user wants the file to be replaced by
     * a newer version
     */
    void resolveConfigFileConflict(const QString &currentPath, bool replace);

private Q_SLOTS:
    void sync();
    void updateProperty(int type, const QDBusVariant &variant);
    void onCallFinished(QDBusPendingCallWatcher *watcher);
    void serviceOwnerChanged(QString name, QString oldOwner, QString newOwner);
    void emitFinished(int exitStatus);
};

}

#endif // QAPT_TRANSACTION_H
