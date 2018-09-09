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

#ifndef QAPT_GLOBALS_H
#define QAPT_GLOBALS_H

#include <QFlags>
#include <QList>
#include <QVariantMap>

namespace QApt
{
    class Package;
   /**
    * Defines the PackageList type, which is a QList of Packages
    */
    typedef QList<Package*> PackageList;

   /**
    * Defines the Group type, which is really just a QString
    */
    typedef QString Group;

   /**
    * Defines the CacheState type, which is used for keeping track of package
    * statuses.
    */
    typedef QList<int> CacheState;

   /**
    * Defines the GroupList type, which is a QList of Groups (QStrings)
    */
    typedef QStringList GroupList;

   /**
    * An enumerator listing all error types that the QApt Worker can throw
    */
    enum ErrorCode {
        /// An invalid/unknown value
        UnknownError = -1,
        /// An error code representing no error
        Success = 0,
        /// Emitted when the APT cache could not be initialized
        InitError,
        /// Emitted when the package cache could not be locked
        LockError,
        /// Emitted when there is not enough disk space for an install
        DiskSpaceError,
        /// Emitted when fetching packages failed
        FetchError,
        /// Emitted when dpkg encounters an error during commit
        CommitError,
        /// Emitted when the user has not given proper authentication
        AuthError,
        /// Emitted when the worker crashes or disappears
        WorkerDisappeared,
        /// Emitted when APT prevents the installation of untrusted packages
        UntrustedError,
        /// Emitted when the APT configuration prevents downloads
        DownloadDisallowedError,
        /// Emitted when the selected package does not exist
        NotFoundError,
        /// Emitted when a .deb package cannot be installed due to an arch mismatch
        WrongArchError,
        /// Emitted when the worker cannot mark packages without broken dependencies
        MarkingError
    };

   /**
    * An enumerator listing reason categories for why package and/or
    * its dependencies cannot be installed
    */
    enum BrokenReason {
        /// An unknown/invalid reason
        UnknownReason         = 0,
        /// Broken because the parent is not installable
        ParentNotInstallable  = 1,
        /// Broken because the candidate version is wrong
        WrongCandidateVersion = 2,
        /// Broken because a dependency won't be installed
        DepNotInstallable     = 3,
        /// Broken because  the package is a virtual package
        VirtualPackage        = 4
    };

    // Not yet used
    enum UpdateImportance {
        UnknownImportance = 1,//
        NormalImportance = 2,//
        CriticalImportance = 3,//
        SecurityImportance = 4//
    };

   /**
    * An enumerator listing screenshot types
    */
    enum ScreenshotType {
        /// An unknown/invalid type
        UnknownType = 0,
        /// A smaller thumbnail of the screenshot
        Thumbnail   = 1,
        /// The full screenshot
        Screenshot  = 2
    };

    enum DependencyType {
        /// Junk type
        InvalidType = 0,
        /// Required to run a package
        Depends = 1,
        /// Required to install a package
        PreDepends = 2,
        /// Suggested to enhance functionality
        Suggests = 3,
        /// Should be present in normal installations, but is not vital
        Recommends = 4,
        /// Conflicts another package
        Conflicts = 5,
        /// Replaces files from another package
        Replaces = 6,
        /// Makes another package obsolete
        Obsoletes = 7,
        /// Breaks another package
        Breaks = 8,
        /// Provides features that enhance another package
        Enhances = 9
    };

    enum RelationType {
        /// A non-versioned dependency on a package
        NoOperand = 0x0,
        /// Depends on any version less than or equal to (<=) the specified version
        LessOrEqual = 0x1,
        /// Depends on any version greater than or equal to (>=) the specified version
        GreaterOrEqual = 0x2,
        /// Depends on any version less than (<) the specified version
        LessThan = 0x3,
        /// Depends on any version greater than (>) the specified version
        GreaterThan = 0x4,
        /// Depends on a version equal to (=) the specified version
        Equals = 0x5,
        /// Depends on any version not equal to (!=) the specified version
        NotEqual = 0x6
    };

    enum MultiArchType {
        /// Junk type
        InvalidMultiArchType = 0,
        /** This package is co-installable with itself, but it must not be used to
         * satisfy the dependency of any package of a different architecture from itself.
         * (Basically, not a multi-arch package)
         */
        MultiArchSame,
        /** The package is @b not co-installable with itself, but should be allowed to
         * satisfy the dependencies of a package of a different arch from itself.
         */
        MultiArchForeign,
        /** This permits the reverse-dependencies of the package to annotate their Depends:
         * field to indicate that a foreign architecture version of the package satisfies
         * the dependencies, but does not change the resolution of any existing dependencies.
         */
        MultiArchAllowed
    };

    /**
     * @brief TransactionRole enumerates the different types of worker transactions
     *
     * @since 2.0
     */
    enum TransactionRole {
        /// The transaction role has not yet been determined
        EmptyRole = 0,
        /// The transaction will run a full update on the package cache
        UpdateCacheRole,
        /// The transaction will fully upgrade the system
        UpgradeSystemRole,
        /// The transaction will commit changes to packages
        CommitChangesRole,
        /// The transaction will download package archives
        DownloadArchivesRole,
        /// The transaction will install a .deb file
        InstallFileRole
    };

    /**
     * @brief Enumerates the data properties of worker transactions
     *
     * @since 2.0
     */
    enum TransactionProperty {
        /// QString, the dbus path of the transaction
        TransactionIdProperty = 0,
        /// int, the user id of the user who initiated the transaction
        UserIdProperty,
        /// int, the TransactionRole of the transaction
        RoleProperty,
        /// int, the TransactionStatus of the transaction
        StatusProperty,
        /// int, the ErrorCode of the transaction
        ErrorProperty,
        /// QString, the locale for the worker to use
        LocaleProperty,
        /// QString, the proxy for the worker to use
        ProxyProperty,
        /// QString, the debconf pipe for the worker to use
        DebconfPipeProperty,
        /// QVariantMap, the packages to be acted upon in the transaction
        PackagesProperty,
        /// bool, whether or not the transaction can be cancelled at the moment
        CancellableProperty,
        /// bool, whether or not the transaction has been cancelled
        CancelledProperty,
        /// int, the exit status of the transaction
        ExitStatusProperty,
        /// bool, whether or not the transaction is paused and waiting
        PausedProperty,
        /// QString, status details from APT
        StatusDetailsProperty,
        /// int, progress as percent, 1-100, 101 if indeterminate
        ProgressProperty,
        /// DownloadProgress, progress for individual files
        DownloadProgressProperty,
        /// QStringList, list of untrusted package names
        UntrustedPackagesProperty,
        /// quint64, download speed in bytes
        DownloadSpeedProperty,
        /// quint64, the estimated time until completion in seconds
        DownloadETAProperty,
        /// QString, the path of the .deb file to be installed
        FilePathProperty,
        /// QString, the string describing the current error in detail
        ErrorDetailsProperty,
        /// int, the frontend capabilities for the transaction
        FrontendCapsProperty
    };

    /**
     * @brief An enum for the statuses of ongoing transactions
     *
     * @since 2.0
     */
    enum TransactionStatus {
        /// The status when a client is still setting pre-run properties
        SetupStatus = 0,
        /// The status while the transaction is waiting for authentication
        AuthenticationStatus,
        /// The status when the transaction is waiting in the queue
        WaitingStatus,
        /// The status when a transaction is waiting for a media change prompt
        WaitingMediumStatus,
        /// The status when waiting for the resolution of a config file conflict
        WaitingConfigFilePromptStatus,
        /// The status when the transaction is waiting for the APT lock
        WaitingLockStatus,
        /// The status when a transaction first starts running
        RunningStatus,
        /// The status when the worker is opening the package cache
        LoadingCacheStatus,
        /// The status when a transaction is downloading archives
        DownloadingStatus,
        /// The status when a transaction is committing changes to the APT cache
        CommittingStatus,
        /// The status when a transaction is complete
        FinishedStatus
    };

    /**
     * @brief An enumeration for transaction exit statuses
     *
     * @since 2.0
     */
    enum ExitStatus {
        ExitSuccess = 0,
        ExitCancelled,
        ExitFailed,
        ExitPreviousFailed,
        ExitUnfinished
    };

    /**
     * @brief An enumeration for download progress states
     *
     * @since 2.0
     */
    enum DownloadStatus {
        /// The item is waiting to be downloaded.
        IdleState = 0,
        /// The item is currently being downloaded.
        FetchingState,
        /// The item has been successfully downloaded.
        DoneState,
        /// An error was encountered while downloading this item.
        ErrorState,
        /// The item was downloaded but its authenticity could not be verified.
        AuthErrorState,
        /** The item was could not be downloaded because of a transient network
         * error (e.g. network down, HTTP 404/403 errors, etc)
         */
        NetworkErrorState
    };

    enum UpgradeType {
        /// An invalid default type
        InvalidUpgrade = 0,
        /// An upgrade that will not install or remove new packages
        SafeUpgrade,
        /// An upgrade that may install or remove packages to accomplish the upgrade
        FullUpgrade
    };

    /// Flags for advertising frontend capabilities
    enum FrontendCaps {
        NoCaps = 0,
        DebconfCap,
        MediumPromptCap,
        ConfigPromptCap,
        UntrustedPromptCap
    };
}

Q_DECLARE_TYPEINFO(QList<int>, Q_MOVABLE_TYPE);

#endif
