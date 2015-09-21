/***************************************************************************
 *   Copyright © 2010-2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
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

#include <QtCore/QHash>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>

#include "globals.h"
#include "package.h"

class pkgSourceList;
class pkgRecords;

namespace QApt {
    class Cache;
    class Config;
    class DebFile;
    class Transaction;
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
    explicit Backend(QObject *parent = 0);

     /**
      * Default destructor
      */
    ~Backend();

    /**
     * Initializes the Apt database for usage. It sets up everything the backend
     * will need to perform all operations. Please note that you @b _MUST_ call
     * this function before doing any further operations in the backend, or else
     * risk encountering undefined behavior.
     *
     * @return @c true if initialization was successful
     * @return @c false if there was a problem initializing. If this is the case,
     * calling Backend methods other than initErrorMessage() will result in
     * undefined behavior, and will likely cause a crash.
     */
    bool init();

    /**
     * In the event that the init() or reloadCache() methods have returned false,
     * this method provides access to the error message from APT explaining why
     * opening the cache failed. This is the only safe method to call after
     * encountering a return false of @c false from init() or reloadCache()
     *
     * @since 2.0
     */
    QString initErrorMessage() const;

   /**
    * Returns whether or not APT is configured for installing packages for
    * additional foreign CPU architectures.
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
     * Returns whether or not events are being compressed for multiple
     * markings. Applications doing custom multiple marking loops can
     * use this function to check whether or not to perform post-marking
     * code.

     * @since 1.3
     */
    bool areEventsCompressed() const;

    /**
     * Repopulates the internal package cache, package list, and group list.
     * Mostly used internally, like after an update or a package installation
     * or removal.
     *
     * @return @c true when the cache reloads successfully. If it returns false,
     * assume that you cannot call any methods other than initErrorMessage()
     * safely.
     */
    bool reloadCache();

    /**
     * Takes a snapshot of the current state of the package cache. (E.g.
     * which packages are marked for removal, install, etc)
     *
     * \return The current state of the cache as a @c CacheState
     */
    CacheState currentCacheState() const;

   /**
     * Gets changes made to the cache since the given cache state.
     *
     * @param oldState The CacheState to compare against
     * @param excluded List of packages to exlude from the check
     *
     * @return A QHash containing lists of changed packages for each
     *         Package::State change flag.
     * @since 1.3
     */
    QHash<Package::State, PackageList> stateChanges(const CacheState &oldState,
                                                    const PackageList &excluded) const;

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
    Package *package(QLatin1String name) const;

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
     * Returns a list of all package origins, as machine-readable strings
     *
     * @return The list of machine-readable origin labels
     *
     * @since 1.4
     */
    QStringList origins() const;

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

    /**
     * Returns the machine-readable name for the origin repository of the given
     * the human-readable name.
     *
     * @return The machine-readable origin label
     */
    QString origin(const QString &originLabel) const;

    /** 
     * @returns the origins for a given @p host
     */
    QStringList originsForHost(const QString& host) const;

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

   /**
    * Returns the last time the APT repository sources have been refreshed/checked
    * for updates. (Either with updateCache() or externally via other tools
    * like apt-get)
    *
    * @returns @c QDateTime The time that the cache was last checked for updates.
    * If this cannot be determined, an invalid QDateTime will be returned,
    * which can be checked with QDateTime::isValid()
    */
    QDateTime timeCacheLastUpdated() const;

    /**
     * Returns the capabilities advertised by the frontend associated with the QApt
     * Backend.
     */
    QApt::FrontendCaps frontendCaps() const;

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

    /**
     * Returns a pointer to the internal package records object. Mainly used
     * for internal purposes in QApt::Package
     *
     * @return the package records object used by the backend
     * @since 1.4
     */
    pkgRecords *records() const;

private:
    Q_DECLARE_PRIVATE(Backend)
    friend class Package;
    friend class PackagePrivate;

    Package *package(pkgCache::PkgIterator &iter) const;

    void setInitError();
    void loadPackagePins();

Q_SIGNALS:
    /**
     * Emitted whenever a package changes state. Useful for knowning when to
     * react to state changes.
     */
    void packageChanged();

    /**
     * Emitted when the apt cache reload is started.
     *
     * After this signal is emitted all @c Package in the backend will be
     * deleted. Therefore, all pointers obtained in precedence from the backend
     * shall not be used anymore. This includes any @c PackageList returned by
     * availablePackages(), upgradeablePackages(), markedPackages() and search().
     *
     * Also @c pkgCache::PkgIterator are invalidated.
     *
     * Wait for signal cacheReloadFinished() before requesting a new list of packages.
     *
     * @see reloadCache()
     */
    void cacheReloadStarted();

    /**
     * Emitted after the apt cache has been reloaded.
     *
     * @see cacheReloadStarted();
     */
    void cacheReloadFinished();

    /**
     * This signal is emitted when a Xapian search cache update is started.
     *
     * Slots connected to this signal can then listen to the xapianUpdateProgress
     * signal for progress updates.
     */
    void xapianUpdateStarted();

    /**
     * This signal is emitted when a Xapian search cache update is finished.
     */
    void xapianUpdateFinished();

    /**
     * Emits the progress of the Apt Xapian Indexer
     *
     * @param percentage The progress percentage of the indexer
     */
    void xapianUpdateProgress(int percentage);

    /**
     * Emitted whenever the QApt Worker's transaction queue has
     * changed.
     *
     * @param active The transaction ID of the active transaction
     * @param queue A list of transaction IDs of all transactions
     * currently in the queue, including the active one.
     *
     * @since 2.0
     */
    void transactionQueueChanged(QString active, QStringList queue);

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
     * Marks multiple packages at once. This is more efficient than marking
     * packages individually, as event compression is utilized to prevent
     * post-marking calculations from being performed until after all packages
     * have been marked.
     *
     * @param packages The list of packages to be marked
     * @param action The action to perform on the list of packages
     *
     * @since 1.3
     */
    void markPackages(const QApt::PackageList &packages, QApt::Package::State action);

    /**
     * Manual control for enabling/disabling event compression. Useful for when
     * an application needs to have its own multiple marking loop, but still wants
     * to utilize event compression
     */
    void setCompressEvents(bool enabled);

    /**
     * Starts a transaction which will commit all pending package state
     * changes that have been made to the backend
     *
     * @return A pointer to a @c Transaction object tracking the commit
     *
     * @since 2.0
     */
    QApt::Transaction *commitChanges();

    /**
     * Starts a transaction which will install the list of provided packages.
     * This function is useful when you only need a few packages installed and
     * don't need to commit a complex set of changes with commitChanges().
     *
     * @param packages The packages to be installed
     *
     * @return A pointer to a @c Transaction object tracking the install
     *
     * @since 2.0
     * @see removePackages
     * @see commitChanges
     */
    QApt::Transaction *installPackages(QApt::PackageList packages);

    /**
     * Starts a transaction which will remove the list of provided packages.
     * This function is useful when you only need a few packages removed and
     * don't need to commit a complex set of changes with commitChanges().
     *
     * @param packages The packages to be removed
     *
     * @return A pointer to a @c Transaction object tracking the removal
     *
     * @since 2.0
     * @see installPackages
     * @see commitChanges
     */
    QApt::Transaction *removePackages(QApt::PackageList packages);

   /**
    * Downloads the packages listed in the provided list file to the provided
    * destination directory.
    *
    * If the list file provided cannot be opened, a null pointer will be returned.
    *
    * @return A pointer to a @c Transaction object tracking the download
    *
    * @param listFile The path to the package list file
    * @param destination The path of the directory to download the packages to
    *
    * @since 2.0
    */
    Transaction *downloadArchives(const QString &listFile, const QString &destination);

   /**
    * Installs a .deb package archive file.
    *
    * If the file has any additional dependencies that are not currently
    * installed, the worker will install these. The backend sends out normal
    * download event signals.
    *
    * When the commit process for the DebFile starts, the backend will emit the
    * QApt::DebInstallStarted worker signal. Similarly, when the commit is
    * finished the backend will emit the QApt::DebInstallFinished signal.
    *
    * @param file the DebFile to install
    *
    * @see workerEvent()
    * @see packageDownloadProgress()
    *
    * @since 2.0
    */
    Transaction *installFile(const DebFile &file);

    /**
     * Starts a transaction that will check for and downloads new package
     * source lists. (Essentially, checking for updates.)
     *
     * @return A pointer to a @c Transaction object tracking the cache update.
     *
     * @since 2.0
     */
    Transaction *updateCache();

    /**
     * Starts a transaction which will upgrade as many of the packages as it can.
     * If the upgrade type is a "safe" upgrade, only packages that can be upgraded
     * without installing or removing new packages will be upgraded. If a "full"
     * upgrade is chosen, the transaction will upgrade all packages, and can
     * install or remove packages to do so.
     *
     * @param upgradeType The type of upgrade to be performed. (safe vs. full)
     *
     * @return A pointer to a @c Transaction object tracking the upgrade
     *
     * @since 2.0
     */
    Transaction *upgradeSystem(QApt::UpgradeType upgradeType);

    /**
     * Exports a list of all packages currently installed on the system. This
     * list can be read by the readSelections() function or by Synaptic.
     *
     * @param path The path to save the selection list to
     *
     * @return @c true if saving succeeded
     * @return @c false if the saving failed
     *
     * @since 1.2
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
     * @return @c true if saving succeeded
     * @return @c false if the saving failed
     *
     * @since 1.2
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
     * @return @c true if reading/marking succeeded
     * @return @c false if the reading/marking failed
     *
     * @since 1.2
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
    *
    * @since 1.2
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
    *
    * @since 1.2
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

    /**
     * Sets the capabilities of the frontend. All transactions created after this
     * is set will inherit these capability flags.
     *
     * @param caps The capabilities to advertise the frontend as having
     *
     * @since 2.1
     */
    void setFrontendCaps(QApt::FrontendCaps caps);

private Q_SLOTS:
    void emitPackageChanged();
    void emitXapianUpdateFinished();
};

}

#endif
