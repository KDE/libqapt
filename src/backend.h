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

#ifndef QAPT_BACKEND_H
#define QAPT_BACKEND_H

#include <QtCore/QVariantMap>

#include "globals.h"
#include "package.h"

class pkgSourceList;

namespace QApt {
    class Cache;
    class Config;
    class DebFile;
}

/**
 * The QApt namespace is the main namespace for LibQApt. All classes in this
 * library fall under this namespace.
 */
namespace QApt {

class BackendPrivate;

/**
 * @brief The main entry point for performing operations with the dpkg database
 *
 * Backend encapsulates all the needed logic to perform most apt operations.
 * It implements the initializing of the database and all requests to/for the
 * database. Please note that you @e MUST call init() before doing any
 * further operations to the backend, or else risk encountering undefined
 * behavior.
 *
 * @author Jonathan Thomas
 */
class Q_DECL_EXPORT Backend : public QObject
{
    Q_OBJECT
public:
     /**
      * Default constructor
      */
    explicit Backend();

     /**
      * Default destructor
      */
    virtual ~Backend();

    /**
     * Initializes the Apt database for usage. It sets up everything the backend
     * will need to perform all operations. Please note that you @b _MUST_ call
     * this function before doing any further operations in the backend, or else
     * risk encountering undefined behavior.
     *
     * @return @c true if initialization was successful
     * @return @c false if there was a problem initializing
     */
    bool init();

   /**
    * Returns whether or not APT is configured for installing packages for
    * additional foriegn CPU architectures.
    */
    bool isMultiArchEnabled() const;

   /**
    * Returns a list of the CPU architectures APT supports
    */
    QStringList architectures() const;

   /**
    * Returns the native CPU architecture of the computer
    */
    QString nativeArchitecture() const;

    /**
     * Returns whether the undo stack is empty
     */
    bool isUndoStackEmpty() const;

    /**
     * Returns whether the redo stack is empty
     *
     */
    bool isRedoStackEmpty() const;

    /**
     * Repopulates the internal package cache, package list, and group list.
     * Mostly used internally, like after an update or a package installation
     * or removal.
     */
    void reloadCache();

    /**
     * Takes a snapshot of the current state of the package cache. (E.g.
     * which packages are marked for removal, install, etc)
     *
     * \return The current state of the cache as a @c CacheState
     */
    CacheState currentCacheState() const;

   /**
     * Returns the last event that the worker reported. When the worker is not
     * running, this returns InvalidEvent
     *
     * \return The last reported @c WorkerEvent of the worker
     *
     * @since 1.1
     */
    WorkerEvent workerState() const;

    /**
     * Pointer to the QApt Backend's config object.
     *
     * \return A pointer to the QApt Backend's config object
     */
    Config *config() const;

    /**
     * Queries the backend for a Package object for the specified name.
     *
     * @b _WARNING_ :
     * Note that if a package with a given name cannot be found, a null pointer
     * will be returned. Also, please note that certain actions like reloading
     * the cache may invalidate the pointer.
     *
     * @param name name used to specify the package returned
     *
     * @return A pointer to a @c Package defined by the specified name
     */
    Package *package(const QString &name) const;

    /** Overload for package(const QString &name) **/
    Package *package(const QLatin1String &name) const;

    /**
     * Queries the backend for a Package object that installs the specified
     * file.
     *
     * @b _WARNING_ :
     * Note that if a package with a given name cannot be found, a null pointer
     * will be returned. Also, please note that certain actions like reloading
     * the cache may invalidate the pointer.
     *
     * @param file The file used to search for the package
     *
     * @return A pointer to a @c Package defined by the specified name
     */
    Package *packageForFile(const QString &file) const;

    /**
     * Returns a list of all package origins, as user readable strings.
     *
     * @return The list of human-readable origin labels
     */
    QStringList originLabels() const;

    /**
     * Returns the human-readable name for the origin repository of the given
     * the machine-readable name.
     *
     * @return The human-readable origin label
     */
    QString originLabel(const QString &origin) const;

    // TODO QApt2: const QString &originLabel
    /**
     * Returns the machine-readable name for the origin repository of the given
     * the human-readable name.
     *
     * @return The machine-readable origin label
     */
    QString origin(QString originLabel) const;

    // TODO QApt2: Around that time it might be wise to use qint64 for count()'s
    /**
     * Queries the backend for the total number of packages in the APT
     * database, discarding no-longer-existing packages that linger on in the
     * status cache (That have a version of 0)
     *
     * @return The total number of packages in the Apt database
     */
    int packageCount() const;

    /**
     * Queries the backend for the total number of packages in the Apt
     * database, discarding no-longer-existing packages that linger on in the
     * status cache (That have a version of 0)
     *
     * @param states The package state(s) for which you wish to count packages for
     *
     * @return The total number of packages of the given PackageState in the
     *         APT database
     */
    int packageCount(const Package::States &states) const;

    /**
     * Queries the backend for the total number of packages in the APT
     * database that are installed.
     *
     * This is quicker than using the
     * packageCount(const Package::States &states) overload, and is
     * the recommended way for getting an installed packages count.
     *
     * @return The number of installed packages in the APT database
     *
     * @since 1.1
     */
    int installedCount() const;

    /**
     * Queries the backend for the total number of packages in the APT
     * database marked for installation.
     *
     * This is quicker than using the
     * packageCount(const Package::States &states) overload, and is
     * the recommended way for checking how many packages are to be
     * installed/upgraded.
     *
     * @return The number of packages marked for installation
     *
     * @since 1.1
     */
    int toInstallCount() const;

    /**
     * Queries the backend for the total number of packages in the APT
     * database marked for removal or purging.
     *
     * This is quicker than using the
     * packageCount(const Package::States &states) overload, and is
     * the recommended way for checking how many packages are to be
     * removed/purged.
     *
     * @return The number of packages marked for removal/purging
     *
     * @since 1.1
     */
    int toRemoveCount() const;

    /**
     * Returns the total amount of data that will be downloaded if the user
     * commits changes. Cached packages will not show up in this count.
     *
     * @return The total amount that will be downloaded in bytes.
     */
    qint64 downloadSize() const;

    /**
     * Returns the total amount of disk space that will be consumed or
     * freed once the user commits changes. Freed space will show up as a
     * negative number.
     *
     * @return The total disk space to be used in bytes.
     */
    qint64 installSize() const;

    /**
     * Returns a list of all available packages. This includes essentially all
     * packages, excluding now-nonexistent packages that have a version of 0.
     *
     * \return A @c PackageList of all available packages in the Apt database
     */
    PackageList availablePackages() const;

    /**
     * Returns a list of all upgradeable packages
     *
     * \return A @c PackageList of all upgradeable packages
     */
    PackageList upgradeablePackages() const;

    /**
     * Returns a list of all packages that have been marked for change. (To be
     * installed, removed, etc)
     *
     * \return A @c PackageList of all packages marked to be changed
     */
    PackageList markedPackages() const;

    /**
     * A quick search that uses the APT Xapian index to search for packages
     * that match the given search string. While it is quite fast in 95% of
     * all cases, the relevancy of its results may in some cases not be 100%
     * accurate. Irrelevant results may slip in, and some relevant results
     * may be cut.
     *
     * You @e must call the openXapianIndex() function before search will work
     *
     * In the future, a "slow" search that searches by exact matches for
     * certain parameters will be implemented.
     *
     * @param searchString The string to narrow the search by.
     *
     * \return A @c PackageList of all packages matching the search string.
     *
     * @see openXapianIndex()
     */
    PackageList search(const QString &searchString) const;

    /**
     * Returns a list of all available groups
     *
     * \return A @c GroupList of all available groups in the Apt database
     */
    GroupList availableGroups() const;

    /**
     * Returns whether the search index needs updating
     *
     * @see updateXapianIndex()
     */
    bool xapianIndexNeedsUpdate() const;

   /**
    * Attempts to open the APT Xapian index, needed for searching
    *
    * \returns true if opening was successful
    * \returns false otherwise
    */
    bool openXapianIndex();

   /**
     * Returns whether there are packages with marked changes waiting to be
     * committed
     */
    bool areChangesMarked() const;

    /**
     * Returns whether the cache has broken packages or has a null dependency
     * cache
     */
    bool isBroken() const;

protected:
    BackendPrivate *const d_ptr;

    /**
     * Returns a pointer to the internal package source list. Mainly used for
     * internal purposes in QApt::Package.
     *
     * @return @c pkgSourceList The package source list used by the backend
     */
    pkgSourceList *packageSourceList() const;

    /**
     * Returns a pointer to the internal package cache. Mainly used for
     * internal purposes in QApt::Package.
     *
     * @return @c pkgSourceList The package cache list used by the backend
     */
    Cache *cache() const;

private:
    Q_DECLARE_PRIVATE(Backend);
    friend class Package;
    friend class PackagePrivate;

    Package *package(pkgCache::PkgIterator &iter) const;

    void throwInitError();

Q_SIGNALS:
    /**
     * Emitted whenever a backend error occurs. You should listen to this
     * signal and present the error/clean up when your app receives it.
     *
     * @param error @c ErrorCode enum member indicating error type
     * @param details  A @c QVariantMap containing containing info about the error, if
     *                available
     */
    void errorOccurred(QApt::ErrorCode error, const QVariantMap &details);

    /**
     * Emitted whenever a backend warning occurs. You should listen to this
     * signal and present the warning when your app receives it.
     *
     * @param error @c WarningCode enum member indicating error type
     * @param details  A @c QVariantMap containing info about the warning, if
     *                available
     */
    void warningOccurred(QApt::WarningCode warning, const QVariantMap &details);

    /**
     * Emitted whenever the worker asks a question. You should listen to this
     * signal and present the question to the user when your app receives it.
     *
     * You should send the response back to the worker as a QVariantMap
     * using the Backend's answerWorkerQuestion() function.
     *
     * @param question A @c QApt::WorkerQuestion enum member indicating question type
     * @param details A @c QVariantMap containing info about the question, if available
     *
     * @see answerWorkerQuestion()
     */
    void questionOccurred(QApt::WorkerQuestion question, const QVariantMap &details);

    /**
     * Emitted whenever a package changes state. Useful for knowning when to
     * react to state changes.
     */
    void packageChanged();

    /**
     * Emitted whenever a backend event occurs.
     *
     * @param event A @c WorkerEvent enum member indicating event type
     */
    void workerEvent(QApt::WorkerEvent event);

    /**
     * Emits total progress information while the QApt Worker is downloading
     * packages.
     *
     * @param percentage Total percent complete
     * @param speed Current download speed in bytes
     * @param ETA Current estimated download time
     */
    void downloadProgress(int percentage, int speed, int ETA);

    /**
     * Emits per-package progress information while the QApt Worker is
     * downloading packages.
     *
     * @param name Name of the package currently being downloaded
     * @param percentage Percentage of the package downloaded
     * @param URI The URI of the download location
     * @param size The size of the download in bytes
     * @param flag Fetch type (is a QApt::Global enum member)
     *
     * @since 1.1
     */
    void packageDownloadProgress(const QString &name, int percentage, const QString &URI,
                                 double size, int flag);

    /**
     * Emitted whenever an item has been downloaded.
     *
     * This signal is deprecated. You should connect to packageDownloadProgress
     * which provides a lot more information about the fetch.
     *
     * @param flag Fetch type (is a QApt::Global enum member)
     * @param message Usually the URI of the item that's being downloaded
     */
    QT_DEPRECATED void downloadMessage(int flag, const QString &message);

    /**
     * Emits the progress of a current package installation/removal/
     * operation.
     *
     * @param status Current status retrieved from dpkg
     * @param percentage Total percent complete
     */
    void commitProgress(const QString &status, int percentage);

   /**
    * Emitted during the install of a .deb file, giving the output
    * of the dpkg process installing the .deb
    *
    * @param message A line of output from dpkg
    *
    * @since 1.2
    *
    * @see installDebFile(const DebFile &debFile)
    */
    void debInstallMessage(const QString &message);

   /**
    * Emits the progress of the Apt Xapian Indexer
    *
    * @param progress The progress percentage of the indexer
    */
    void xapianUpdateProgress(int percentage);

public Q_SLOTS:
   /**
    * Sets the maximum size of the undo and redo stacks.
    * The default size is 20.
    *
    * @param newSize The new size of the undo/redo stack
    *
    * @since 1.1
    */
    void setUndoRedoCacheSize(int newSize);

    /**
     * Takes the current state of the cache and puts it on the undo stack
     */
    void saveCacheState();

    /**
     * Restores the package cache to the given state.
     *
     * @param state The state to restore the cache to
     */
    void restoreCacheState(const CacheState &state);

    /**
     * Un-performs the last action performed to the package cache
     */
    void undo();

    /**
     * Re-performs the last un-done action to the package cache.
     */
    void redo();

    /**
     * Marks all upgradeable packages for upgrading, without marking new
     * packages for installation.
     */
    void markPackagesForUpgrade();

    /**
     * Marks all upgradeable packages for upgrading, including updates that
     * would require marking new packages for installation.
     */
    void markPackagesForDistUpgrade();

   /**
    * Marks all packages that are autoremoveable, as determined by APT. In
    * general these are packages that were automatically installed that now
    * no longer have any packages dependening on them. (Like after a
    * library transition libqapt0 -> libqapt1)
    *
    * @since 1.1
    */
    void markPackagesForAutoRemove();

    /**
     * Marks a package for install.
     *
     * @param name The name of the package to be installed
     */
    void markPackageForInstall(const QString &name);

    /**
     * Marks a package for removal.
     *
     * @param name The name of the package to be removed
     */
    void markPackageForRemoval(const QString &name);

    /**
     * Commits all pending package state changes that have been made.
     *
     * This function is asynchronous. Events from the worker that
     * occur while committing changes can be tracked with the workerEvent()
     * signal.
     *
     * Commit progress can be tracked with the commitProgress() signal
     *
     * @see workerEvent()
     * @see commitProgress()
     */
    void commitChanges();

   /**
    * Downloads the packages listed in the provided list file to the provided
    * destination directory. The worker sends normal download event signals
    * as usual, and this can be handled exactly like any other package download
    *
    * @param listFile The path to the package list file
    * @param destination The path of the directory to download the packages to
    */
    void downloadArchives(const QString &listFile, const QString &destination);

    void installDebFile(const DebFile &file);

    /**
     * A slot that Packages use to tell the backend they've changed.
     * (Used internally by QApt::Package. You likely will never use this)
     */
    void packageChanged(Package *package);

    /**
     * Checks for and downloads new package source lists.
     *
     * This function is asynchronous. Worker events that occur while
     * donwloading cache files can be tracked with the workerEvent() signal.
     *
     * Overall download progress can be tracked by the downloadProgress()
     * signal, and per-package download progress can be tracked by the
     * packageDownloadProgress() signal.
     *
     * @see workerEvent()
     * @see downloadProgress()
     * @see packageDownloadProgress()
     */
    void updateCache();

    /**
     * Cancels download operations in the worker initialized by the
     * updateCache() or commitChanges() functions. This function
     * will only have an effect if a download operation is in progress.
     * The actual committing of changes cannot be canceled once in progress.
     *
     * This function is asynchronous. The backend will report a
     * \c UserCancelError using the errorOccurred() signal
     *
     * @see errorOccurred()
     */
    void cancelDownload();

    /**
     * This function should be used to return the answer the user has given
     * to a worker question delivered by the questionOccurred() signal
     *
     * @see questionOccurred()
     */
    void answerWorkerQuestion(const QVariantMap &response);

    /**
     * Exports a list of all packages currently installed on the system. This
     * list can be read by the readSelections() function or by Synaptic.
     *
     * @param path The path to save the selection list to
     *
     * \return @c true if saving succeeded
     * \return @c false if the saving failed
     *
     * @since 1.1
     *
     * @see loadSelections()
     * @see saveSelections()
     */
    bool saveInstalledPackagesList(const QString &path) const;

    /**
     * Writes a list of packages that have been marked for install, removal or
     * upgrade.
     *
     * @param path The path to save the selection list to
     *
     * \return @c true if saving succeeded
     * \return @c false if the saving failed
     *
     * @see saveInstalledPackagesList()
     * @see loadSelections()
     */
    bool saveSelections(const QString &path) const;

    /**
     * Reads and applies selections from a text file generated from either
     * saveSelections() or from Synaptic
     *
     * @param path The path from which to read the selection list
     *
     * \return @c true if reading/marking succeeded
     * \return @c false if the reading/marking failed
     *
     * @see saveSelections()
     * @see saveInstalledPackagesList()
     */
    bool loadSelections(const QString &path);

   /**
    * Writes a list of packages that have been marked for installation. This
    * list can then be loaded with the loadDownloadList() function to start
    * downloading the packages.
    *
    * @param path The path to save the download list to
    *
    * @return @c true if savign succeeded, @c false if the saving failed
    */
    bool saveDownloadList(const QString &path) const;

   /**
    * Locks the package at either the current version if installed, or
    * prevents automatic installation if not installed.
    *
    * The backend must be reloaded before the pinning will take effect
    *
    * @param package The package to control pinning for
    * @param pin Whether to pin or unpin the package
    *
    * @return @c true on success, @c false on failure
    */
    bool setPackagePinned(QApt::Package *package, bool pin);

   /**
    * Tells the QApt Worker to initiate a rebuild of the Xapian package search
    * index.
    *
    * This function is asynchronous. The worker will report start and finish
    * events using the workerEvent() signal. Progress is reported by the
    * xapianUpdateProgress() signal.
    *
    * @see workerEvent()
    * @see xapianUpdateProgress()
    * @see xapianIndexNeedsUpdate()
    */
    void updateXapianIndex();

   /**
    * Add the given .deb package archive to the APT package cache.
    *
    * To succeed, the .deb file's corresponding package must already be known
    * to APT. The version of the package that the .deb file provides must match
    * the candidate version from APT, and additionally the md5 sums of the .deb
    * file and the candidate version of the package in APT must match.
    *
    * The main use for this function is to add .deb archives from e.g. a USB
    * stick so that computers without internet connections can install/upgrade
    * packages.
    *
    * @param archive The .deb archive to be added to the package cache
    *
    * @return @c true on success, @c false on failure
    */
    bool addArchiveToCache(const DebFile &archive);

private Q_SLOTS:
    void serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void workerStarted();
    void workerFinished(bool result);

    void emitErrorOccurred(int errorCode, const QVariantMap &details);
    void emitWarningOccurred(int warningCode, const QVariantMap &details);
    void emitWorkerEvent(int event);
    void emitWorkerQuestionOccurred(int question, const QVariantMap &details);
};

}

#endif
