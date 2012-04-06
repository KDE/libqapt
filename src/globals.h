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

#include <QtCore/QFlags>
#include <QtCore/QList>
#include <QtCore/QVariantMap>

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
    * Defines the Error type, a QVariantMap with info about errors
    * 
    * These are the fields that each ErrorCode can have:
    *            <"Key", ValueType> (Description of value)
    * InitError: <"ErrorText", QString> (APT's error description)
    *            <"FromWorker, bool> (Whether or not an init error comes from the worker.
    *                                 You want to quit your app if it doesn't)
    * LockError: No fields
    * DiskSpaceError: <"DirectoryString", QString>
    * FetchError: No fields
    * CommitError: <"FailedItem, QString> (Package that failed to commit)
    *              <"ErrorText", QString> (APT's error description)
    * AuthError: No fields
    * WorkerDisappeared: No fields
    * UntrustedError: <"UntrustedItems", QStringList> (List of untrusted packages)
    * UserCancelError: No fields, pseudo-error
    * DownloadDisallowedError: No fields
    * NotFoundError: <"NotFoundString", QString> (String of the nonexistent package)
    *                <"WarningText", QString> (APT's warning description)
    * WrongArchError: <"RequestedArch", QString> (String of the unsupported architecture)
    */
    typedef QVariantMap Error;

    /**
    * Defines the Warning type, a QVariantMap with info about warnings
    *
    * These are the fields that each WarningCode can have:
    *                      <"Key", ValueType> (Description of value)
    * SizeMismatchWarning: No fields
    * FetchFailedWarning: <"FailedItem", QString> (Package that failed to download)
    * 
    */
    typedef QVariantMap Warning;

   /**
    * Defines the Warning type, a QVariantMap with info about worker questions
    *
    * These are the fields that each WorkerQuestion can have:
    *                 <"Key", ValueType> (Description of value)
    * ConfFilePrompt: <"OldConfFile", QString> (Old conf file)
    *                 <"NewConfFile", QString> (New conf file)
    * MediaChange: <"Media", QString> (Name of the CD needed)
    *              <"Drive", QString> (The drive to insert the CD into)
    */
    typedef QVariantMap Question;

   /**
    * An enumerator listing all the events that the QApt Worker can emit
    */
    enum WorkerEvent {
        /// An invalid event
        InvalidEvent            = 0,
        /// The worker has begun to check for updates
        CacheUpdateStarted      = 1,
        /// The worker has finished checking for updates
        CacheUpdateFinished     = 2,
        /// The worker has started downloading packages for an install
        PackageDownloadStarted  = 3,
        /// The worker has finished downloading packages
        PackageDownloadFinished = 4,
        /// The worker has begun committing changes to the package cache
        CommitChangesStarted    = 5,
        /// The worker has finished committing changes
        CommitChangesFinished   = 6,
        /// The worker has begun to update the xapian index
        XapianUpdateStarted     = 7,
        /// The worker has finished updating the xapian index
        XapianUpdateFinished    = 8,
        /// The worker has started installing a .deb with dpkg
        DebInstallStarted       = 9,
        /// The worker has finished installing a .deb with dpkg
        DebInstallFinished      = 10
    };

   /**
    * An enumerator listing all the question types that the QApt Worker can ask
    */
    // TODO: QApt2: Rename to QuestionCode
    enum WorkerQuestion {
        /// An invalid question
        InvalidQuestion  = 0,
        /// Emitted when a configuration file has been changed in an update
        ConfFilePrompt   = 1,
        /// Emitted to request a CD-ROM change or the like
        MediaChange      = 2,
        /// Emitted to check whether or not to install untrusted packages
        InstallUntrusted = 3
    };

   /**
    * An enumerator listing all error types that the QApt Worker can throw
    */
    enum ErrorCode {
        /// An invalid/unknown value
        UnknownError            = 0,
        /// Emitted when the APT cache could not be initialized
        InitError               = 1,
        /// Emitted when the package cache could not be locked
        LockError               = 2,
        /// Emitted when there is not enough disk space for an install
        DiskSpaceError          = 3,
        /// Emitted when fetching packages failed
        FetchError              = 4,
        /// Emitted when dpkg encounters an error during commit
        CommitError             = 5,
        /// Emitted when the user has not given proper authentication
        AuthError               = 6,
        /// Emitted when the worker crashes or disappears
        WorkerDisappeared       = 7,
        /// Emitted when APT prevents the installation of untrusted packages
        UntrustedError          = 8,
        /// A pseudo-error emitted when the user cancels a download
        UserCancelError         = 9,
        /// Emitted when the APT configuration prevents downloads
        DownloadDisallowedError = 10,
        /// Emitted when the selected package does not exist
        NotFoundError           = 11,
        /// Emitted when a .deb package cannot be installed due to an arch mismatch
        WrongArchError          = 12
    };

   /**
    * An enumerator listing all warning types that the QApt Worker can throw
    */
    enum WarningCode {
        /// An invalid/unknown warning
        UnknownWarning        = 0,
        /// Emitted when the downloaded size didn't match the expected size
        SizeMismatchWarning   = 1,
        /// Emitted for non-fatal fetch errors
        FetchFailedWarning    = 2
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

   /**
    * An enumerator listing all types of fetches the QApt Worker can do
    */
    enum FetchType {
        /// An invalid enum value
        InvalidFetch = 0,
        /// A normal download fetch
        DownloadFetch = 1,
        /// A fetch that happens when the cached item equals the item on the sever
        HitFetch = 2,
        /// A fetch that happens when an item is ignored
        IgnoredFetch = 3,
        /// A fetch that really just queues an item for download
        QueueFetch = 4
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
        /* This package is co-installable with itself, but it must not be used to
         * satisfy the dependency of any package of a different architecture from itself.
         * (Basically, not a multi-arch package)
         */
        MultiArchSame,
        /* The package is @b not co-installable with itself, but should be allowed to
         * satisfy the dependencies of a package of a different arch from itself.
         */
        MultiArchForeign,
        /* This permits the reverse-dependencies of the package to annotate their Depends:
         * field to indicate that a foreign architecture version of the package satisfies
         * the dependencies, but does not change the resolution of any existing dependencies.
         */
        MultiArchAllowed
    };
}

#endif
