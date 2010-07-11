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

namespace QApt
{
    class Package;
   /**
    * Defines the PackageList type, which is a QList of Packages
    */
    typedef QList<Package*> PackageList;

   /**
    * Defines the GroupList type, which is really just a QString
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
        XapianUpdateFinished    = 8
    };

   /**
    * An enumerator listing all the question types that the QApt Worker can ask
    */
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
        DownloadDisallowedError = 10
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
        IgnoredFetch = 3
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
}

#endif
