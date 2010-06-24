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

namespace QApt
{
   class Package;
   class Group;
   /**
    * Defines the PackageList type, which is a QList of Packages
    */
    typedef QList<Package*> PackageList;

   /**
    * Defines the GroupList type, which is a QList of Groups
    */
    typedef QList<Group*> GroupList;

    enum WorkerEvent {
        InvalidEvent            = 0,
        CacheUpdateStarted      = 1,
        CacheUpdateFinished     = 2,
        PackageDownloadStarted  = 3,
        PackageDownloadFinished = 4,
        CommitChangesStarted    = 5,
        CommitChangesFinished   = 6,
    };

    enum WorkerQuestion {
        InvalidQuestion  = 0,
        ConfFilePrompt   = 1,
        DebconfPrompt    = 2, //TODO in worker
        MediaChange      = 3,
        InstallUntrusted = 4
    };

    enum ErrorCode {
        UnknownError            = 0,
        InitError               = 1,
        LockError               = 2,
        DiskSpaceError          = 3,
        FetchError              = 4,
        CommitError             = 5,
        AuthError               = 6,
        WorkerDisappeared       = 7,
        UntrustedError          = 8,
        UserCancelError         = 9,
        DownloadDisallowedError = 10
    };

    enum WarningCode {
        UnknownWarning        = 0,
        SizeMismatchWarning   = 1
    };

    enum FetchType {
        InvalidFetch = 0,
        DownloadFetch = 1,
        HitFetch = 2,
        IgnoredFetch = 3
    };

    enum UpdateImportance {
        UnknownImportance = 1,//
        NormalImportance = 2,//
        CriticalImportance = 3,//
        SecurityImportance = 4//
    };

    enum ScreenshotType {
        UnknownType = 0,
        Thumbnail   = 1,
        Screenshot  = 2
    };
}

#endif
