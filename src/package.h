/***************************************************************************
 *   Copyright © 2010-2011 Jonathan Thomas <echidnaman@kubuntu.org>        *
 *   Heavily inspired by Synaptic library code ;-)                         *
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

#ifndef QAPT_PACKAGE_H
#define QAPT_PACKAGE_H

#include <QFlags>
#include <QUrl>
#include <QDateTime>
#include <QVariantMap>

#include <apt-pkg/pkgcache.h>

#include "dependencyinfo.h"
#include "globals.h"

namespace QApt {

class Backend;
class MarkingErrorInfo;

/**
 * PackagePrivate is a class containing all private members of the Package class
 */
class PackagePrivate;

/**
 * The Package class is an object for referencing a software package in the Apt
 * package database. You will be getting most of your information about your
 * packages from this class.
 *
 * @author Jonathan Thomas
 */
class Q_DECL_EXPORT Package
{

public:
   /**
    * Destructor.
    */
    ~Package();

   /**
    * Returns the name of the package
    *
    * \return The name of the package
    */
    QLatin1String name() const;

   /**
    * Returns the unique internal identifier for the package
    *
    * \return The identifier of the package
    */
    int id() const;

   /**
    * Returns the version of the package, regardless of whether it is installed
    * or not. If not installed, it returns the version of the candidate for
    * installation, which may not necessarily be the latest. (If the version has
    * been changed with setVersion())
    *
    * \return The version of the package
    *
    * \sa Package::setVersion()
    */
    QString version() const;

   /**
    * Returns the upstream version of the package. This is the Debian version
    * with the epoch and Debian revision information (if any) removed.
    *
    * (E.g. 0.2-0ubuntu1 becomes 0.2)
    *
    * @return The upstream version of the package
    *
    * @since 1.2
    */
    QString upstreamVersion() const;

   /**
    * Returns the upstream portion of @c version
    *
    * @param version The version to get the upstream portion from
    *
    * @return The upstream version from the given version string
    */
    static QString upstreamVersion(const QString &version);

   /**
    * Returns the CPU architecture that the package supports
    *
    * @return The package's architecture
    */
    QString architecture() const;

   /**
    * Returns a list of all available versions of the package in the form of
    * "version, release" (E.g. "0.2-0ubuntu1, maverick")
    *
    * \return All available versions of the package
    */
    QStringList availableVersions() const;

   /**
    * Returns the categorical section where the package resides
    *
    * \return The section of the package
    */
    QLatin1String section() const;

   /**
    * Returns the source package corresponding to the package
    *
    * \return The source package of the package
    */
    QString sourcePackage() const;

   /**
    * Returns the short description (or "summary" in libapt-pkg terms) of the
    * package
    *
    * \return The short description of the package
    */
    QString shortDescription() const;

   /**
    * Returns the maintainer of the package
    *
    * \return The maintainer of the package
    */
    QString maintainer() const;

   /**
    * Returns the homepage of the package
    *
    * \return The homepage of the package
    */
    QString homepage() const;

   /**
    * Returns the installed version of the package.
    *
    * \return The installed version of the package. If this package is not
    *         installed, this function will return a null @c QString
    */
    QString installedVersion() const;

   /**
    * Returns the newest available version of the package if it is not
    * installed.
    *
    * \return The available version of the package. If this package is
    *         installed, this function will return a null @c QString.
    */
    QString availableVersion() const;

   /**
    * Returns the priority of the package
    *
    * \return The priority of the package
    */
    QString priority() const;

   /**
    * Returns the files that this package has installed. 
    *
    * \return The file list of the package. If the package is not installed, it
    *         will return an empty list.
    */
    QStringList installedFilesList() const;

   /**
    * Returns the long description of the package.
    *
    * \return The long description of the package
    */
    QString longDescription() const;

   /**
    * Returns the origin of the package.
    * (Usually Ubuntu or Debian)
    *
    * \return The origin of the package
    */
    QString origin() const;

   /**
    * Returns the site the package comes from.
    * (e.g. archive.ubuntu.com)
    *
    * \return The site the package originates from
    */
    QString site() const;

   /**
    * Returns a list of archives that the candidate version of the package is
    * available from.
    * (E.g. oneiric, oneiric-updates, sid, etc)
    *
    * @return The origin of the package
    *
    * @since 1.3
    */
    QStringList archives() const;

   /**
    * Returns the archive component of the package. (E.g. main, restricted,
    * universe, contrib, etc)
    *
    * \return The archive component of the package
    */
    QString component() const;

   /**
    * Returns the md5sum of the candidate version of the package
    *
    * @return The md5sum of the package
    */
    QByteArray md5Sum() const;

   /**
    * Returns the url to the location of a package's changelog.
    *
    * \return The location of the package's changelog
    */
    QUrl changelogUrl() const;

   /**
    * Returns the url of the package's screenshot over the Internet.
    *
    * @param type The type of screenshot to be fetched as a QApt::ScreenshotType
    *
    * \return The url of the package changelog
    */
    QUrl screenshotUrl(QApt::ScreenshotType type) const;

   /**
    * Returns the date when Canonical's support of the package ends.
    *
    * \return The date that the package is supported until. If it is not
    *         supported now, then it will return an invalid date.
    */
    QDateTime supportedUntil() const;

   /**
    * Returns the specified field of the package's debian/control file
    *
    * This function can be used to return data from custom control fields
    * which do not have an official function inside APT to retrieve them.
    * 
    * For example, the supportedUntil() function uses this function to
    * retrieve the value of the "Supported" field, which is Ubuntu-specific
    * and does not have an APT function with which to obtain it.
    *
    * Another usecase is the GStreamer metadata fields for GStreamer packages,
    * which are used to give information on what mimetypes/GStreamer version
    * the package supports.
    *
    * @since 1.1
    */
    QString controlField(QLatin1String name) const;

    /** Overload for QString controlField(QLatin1String name) const; **/
    QString controlField(const QString &name) const;

   /**
    * Returns the amount of hard drive space that the currently-installed
    * version of this package takes up.
    * This is human-unreadable, so KDE applications may wish to run this
    * through the KFormat().formatByteSize() function to get a
    * localized, human-readable number.
    *
    * Returns -1 on error.
    *
    * \return The installed size of the package
    */
    qint64 currentInstalledSize() const;

   /**
    * Returns the amount of hard drive space that this package will take up
    * once installed.
    * This is human-unreadable, so KDE applications may wish to run this
    * through the KFormat().formatByteSize() function to get a
    * localized, human-readable number.
    *
    * Returns -1 on error.
    *
    * \return The installed size of the package
    */
    qint64 availableInstalledSize() const;

/**
    * Returns the amount of hard drive space that this package takes up
    * when installed.
    * If the package is installed, then it is the size of the currently
    * installed version. Otherwise, it is the size of the candidate
    * version.
    * This is human-unreadable, so KDE applications may wish to run this
    * through the KFormat().formatByteSize() function to get a
    * localized, human-readable number.
    *
    * Returns -1 on error.
    *
    * @return The installed size of the package
    *
    * @see currentInstalledSize()
    * @see availableInstalledSize()
    *
    * @since 3.1
    */
    qint64 installedSize() const;

   /**
    * Returns the download size of the package archive in bytes.
    * This is human-unreadable, so KDE applications may wish to run this
    * through the KFormat().formatByteSize() function to get a
    * localized, human-readable number.
    *
    * Returns -1 on error.
    *
    * \return The installed size of the package
    */
    qint64 downloadSize() const;

   /**
    * Returns the state of a package, using the @b PackageState enum to define
    * states.
    *
    * \return The PackageState flags of the package as an @c int
    */
    int state() const;

   /**
    * Compares v1 with v2 and returns an integer less than, equal to, or
    * greater than zero if s1 is less than, equal to, or greater than s2.
    *
    * @since 1.2
    */
    static int compareVersion(const QString &v1, const QString &v2);

   /**
    * Returns whether the Package is installed
    */
    bool isInstalled() const;

   /**
    * Returns whether the package is supported by Canonical
    */
    bool isSupported() const;

    /**
     * Check whether the candidate version for an update should actually be
     * installed. This is based on an optional Phased-Update-Percentage control
     * field specifying a number between 0 and 100 indicating how many systems
     * should get this update.
     *
     * Whether or not a system is in the update phase is determined by a
     * repeatable discrete random number calculation.
     *
     * @returns @c false if a candidate definitely does not fall into the update
     *          phase or there is no candidate. @c true is returned in all other cases.
     *
     * @warning this function uses statics and is not in the least way threadsafe
     *          nor reentrant.
     *
     * @since 3.1
     */
    bool isInUpdatePhase() const;

   /**
    * A package prepared for MultiArch can have any of three MultiArch "states"
    * that control how dpkg treats the package as a dependency. A package can
    * either be MultiArch: same, MultiArch: foreign, or MultiArch: Allowed.
    *
    * MultiArch: same:
    * - This package is co-installable with itself, but it must not be used to
    *   satisfy the dependency of any package of a different architecture from itself.
    *   (Basically, this package is not multiarch)
    *
    * MultiArch: foreign:
    * - The package is @b not co-installable with itself, but should be allowed to
    *   satisfy the dependencies of a package of a different arch from itself.
    *
    * MultiArch: allowed:
    * - This permits the reverse-dependencies of the package to annotate their Depends:
    *   field to indicate that a foreign architecture version of the package satisfies
    *   the dependencies, but does not change the resolution of any existing dependencies.
    *
    * @return a @c QString for the package's MultiArch state
    *
    * @see multiArchType()
    *
    * @since 1.4
    */
    QString multiArchTypeString() const;

    /**
     * A package prepared for MultiArch can have any of three MultiArch "states"
     * that control how dpkg treats the package as a dependency. A package can
     * either be MultiArch: same, MultiArch: foreign, or MultiArch: Allowed.
     *
     * @return a @c MultiArchType for the package's MultiArch state
     *
     * @see multiArchTypeString()
     *
     * @since 1.4
     */
    MultiArchType multiArchType() const;

    /**
     * Returns whether or not a package is a foreign-arch version of a package
     * that also has a native-architecture counterpart (a "duplicate")
     *
     * This includes installed packages, which are always considered
     * "interesting".
     *
     * @return @c true when the same package is available for the native arch
     * @return @c false when the package is a unique foreign-arch package
     *
     * @since 1.4
     */
    bool isMultiArchDuplicate() const;

   /**
    * Returns whether or not the package is for the native CPU architecture
    */
    bool isForeignArch() const;

    /// Returns a list of DependencyItems that this package depends on.
    QList<DependencyItem> depends() const;

    /// Returns a list of DependencyItems that required to install this package.
    QList<DependencyItem> preDepends() const;

    /// Returns a list of DependencyItems that this package suggests to be installed.
    QList<DependencyItem> suggests() const;

    /// Returns a list of DependencyItems that this package recommends to be installed.
    QList<DependencyItem> recommends() const;

    /// Returns a list of DependencyItems that conflict with this package
    QList<DependencyItem> conflicts() const;

    /// Returns a list of DependencyItems that this package replaces.
    QList<DependencyItem> replaces() const;

    /// Returns a list of DependencyItems that this package obsoletes.
    QList<DependencyItem> obsoletes() const;

    /// Returns a list of DependencyItems that this package breaks.
    QList<DependencyItem> breaks() const;

    /// Returns a list of DependencyItems that this package enhances.
    QList<DependencyItem> enhances() const;

   /**
    * Returns a display-ready list of the names of all the dependencies of this package.
    *
    * \return A \c QStringList of packages that this package depends on
    */
    QStringList dependencyList(bool useCandidateVersion) const;

   /**
    * Returns a list of the names of all the packages that depend on this
    * package. (Reverse dependencies)
    *
    * \return A \c QStringList of packages that depend on this package
    */
    QStringList requiredByList() const;

   /**
    * Returns a list of the names of all the virtual packages that this package
    * provides.
    *
    * \return A \c QStringList of packages that this package provides
    */
    QStringList providesList() const;

   /**
    * Returns a list of the names of all the packages that this package recommends.
    *
    * \return A \c QStringList of packages that this package recommends
    */
    QStringList recommendsList() const;

   /**
    * Returns a list of the names of all the packages that this package suggests.
    *
    * \return A \c QStringList of packages that this package suggests
    */
    QStringList suggestsList() const;

   /**
    * Returns a list of the names of all the packages that this package enhances.
    *
    * \return A \c QStringList of packages that this package enhances
    */
    QStringList enhancesList() const;

   /**
    * Returns a list of the names of all the packages that enhance this package.
    *
    * \return A \c QStringList of packages that enhance this package
    */
    QStringList enhancedByList() const;

   /**
    * If a package is in a broke state, this function returns a why the package
    * is broken by showing all errors in the dependency cache that marking the
    * package has caused.
    */
    QList<QApt::MarkingErrorInfo> brokenReason() const;

   /**
    * Returns whether the package is signed with a trusted GPG signature.
    */
    bool isTrusted() const;

   /**
    * Returns whether the package would break if the current potential changes
    * are committed
    */
    bool wouldBreak() const;

   /**
    * Sets and unsets the auto-install flag
    */
    void setAuto(bool flag = true);

   /**
    * Marks the package to be kept
    */
    void setKeep();

   /**
    * Marks the package for installation
    */
    void setInstall();

   /**
    * Member function that sets whether or not the package needs
    * reinstallation, based on a boolean value passed to it.
    */
    void setReInstall();

   /**
    * Marks the package for removal.
    */
    void setRemove();

   /**
    * Marks the package for complete removal, including config files.
    */
    void setPurge();

   /**
    * Overrides the candidate version, setting it to the version string
    */
    bool setVersion(const QString & version);

   /**
    * Set the package as pinned internally for display purposes.
    *
    * To actually pin a package use @c Backend::setPackagePinned
    *
    * @since 1.2
    * @see Backend::setPackagePinned()
    */
    void setPinned(bool pin);

   /**
    * An enumerator for various states that a @c Package may hold. A package
    * may hold several states at once.
    */
    enum State {
        /// The package will not be changed
        ToKeep              = 1 << 0,
        /// The package has been marked for install
        ToInstall           = 1 << 1,
        /// The package is a new install, never have been installed before
        NewInstall          = 1 << 2,
        /// The package has been marked for reinstall
        ToReInstall         = 1 << 3,
        /// The package has been marked for upgrade
        ToUpgrade           = 1 << 4,
        /// The package has been marked for downgrade
        ToDowngrade         = 1 << 5,
        /// The package has been marked for removal
        ToRemove            = 1 << 6,
        /// The package has been held from being upgraded
        Held                = 1 << 7,
        /// The package is currently installed
        Installed           = 1 << 8,
        /// The package is currently upgradeable
        Upgradeable         = 1 << 9,
        /// The package is currently broken
        NowBroken           = 1 << 10,
        /// The package's install is broken
        InstallBroken       = 1 << 11,
        /// This package is a dependency of another package that is not installed
        Orphaned            = 1 << 12,//
        /// The package has been manually prevented from upgrade
        Pinned              = 1 << 13,//
        /// The package is new in the archives
        New                 = 1 << 14,//
        /// The package still has residual config. (Was not purged)
        ResidualConfig      = 1 << 15,
        /// The package is no longer downloadable
        NotDownloadable     = 1 << 16,
        /// The package has been marked for purging
        ToPurge             = 1 << 17,
        /// The package is essential for a base installation
        IsImportant         = 1 << 18,
        /// The package has had its candidate version overridden by setVersion()
        OverrideVersion     = 1 << 19,
        /// The package was automatically installed as a dependency
        IsAuto              = 1 << 20,
        /// The package is invalid
        IsGarbage           = 1 << 21,
        /// The package's policy is broken
        NowPolicyBroken     = 1 << 22,
        /// The package's install policy is broken
        InstallPolicyBroken = 1 << 23,
        /// The package is not installed
        NotInstalled        = 1 << 24,
        /// The package has been pinned
        IsPinned            = 1 << 25,
        /// The package was auto-marked as a recommend, but then manually held
        IsManuallyHeld      = 1 << 26
    };
    Q_DECLARE_FLAGS(States, State)

private:
    PackagePrivate *const d;

    /**
     * Internal constructor.
     *
     * @param parent The backend that this package is being made a child of
     * @param packageIter The underlying object representing the package in APT
     */
     Package(QApt::Backend* parent, pkgCache::PkgIterator &packageIter);

    /**
     * Returns the internal APT representation of the package
     *
     * \return The interal APT package pointer
     */
     const pkgCache::PkgIterator &packageIterator() const;

     /**
      * Returns a set of state flags that won't change until the next
      * cache reload, and excluding any flags that are able to change.
      * Used internally to avoid having to calculate mutable flags when we know
      * the flag we want to check is immutable.
      */
     int staticState() const;

     friend class Backend;
};

/**
 * Defines the StateChanges type, which is a QHash of Package States
 * and QLists of packages which have those states.
 */
typedef QHash<Package::State, PackageList> StateChanges;

}

#endif
