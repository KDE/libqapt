/***************************************************************************
 *   Copyright © 2011 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#ifndef QAPT_CHANGELOG_H
#define QAPT_CHANGELOG_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QSharedDataPointer>

namespace QApt {

class ChangelogEntryPrivate;
class ChangelogPrivate;

/**
 * The ChangelogEntry class represents a single entry in a Debian changelog file
 *
 * @author Jonathan Thomas
 * @since 1.3
 */

class Q_DECL_EXPORT ChangelogEntry
{
public:
    /**
     * Constructs a Debian changelog entry with the provided entry data for
     * the specified source package
     *
     * @param entryData The raw changelog entry
     * @param sourcePackage The source package the changelog entry is for
     */
    ChangelogEntry(const QString &entryData, const QString &sourcePackage);

    /// Copy constructor
    ChangelogEntry(const ChangelogEntry &other);

    /// Destructor
    ~ChangelogEntry();

    /// Assignment operator
    ChangelogEntry &operator=(const ChangelogEntry &rhs);

    /// Returns the raw text that makes up the changelog entry
    QString entryText() const;

    /// Returns the version of the source package that this changelog entry is for.
    QString version() const;

    /// Returns the timestamp of the changelog entry.
    QDateTime issueDateTime() const;

    /**
     * Returns the description portion of the changelog entry, minus any formatting
     * data such as source package, version, issue time, etc.
     */
    QString description() const;

    /**
     * Returns a list of URLs to webpages providing descriptions of Common
     * Vulnerabilities and Exposures that the version of a package that this
     * changelog entry describes fixes.
     */
    QStringList CVEUrls() const;

private:
    QSharedDataPointer<ChangelogEntryPrivate> d;
};

typedef QList<ChangelogEntry> ChangelogEntryList;


/**
 * The Changelog class is an interface for retreiving information from a Debian
 * changelog file
 *
 * @author Jonathan Thomas
 * @since 1.3
 */
class Q_DECL_EXPORT Changelog
{
public:
    /**
     * Constructor
     *
     * @param data The full contents of a Debian changelog file
     * @param sourcePackage The source package the changelog file is for
     */
    Changelog(const QString &data, const QString &sourcePackage);

    /// Copy constructor
    Changelog(const Changelog &other);

    /// Destructor
    ~Changelog();

    /// Assignment operator
    Changelog &operator=(const Changelog &rhs);

    /// Returns the full changelog as a string
    QString text() const;

    /// Returns a list of parsed individual changelog entries
    ChangelogEntryList entries() const;

    /**
     * Returns a list of parsed individual changelog entries that have been
     * made since the specified version.
     */
    ChangelogEntryList newEntriesSince(const QString &version) const;

private:
    QSharedDataPointer<ChangelogPrivate> d;
};

}

Q_DECLARE_TYPEINFO(QApt::ChangelogEntry, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QApt::Changelog, Q_MOVABLE_TYPE);

#endif
