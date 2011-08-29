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

#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSharedDataPointer>

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
    explicit ChangelogEntry(const QString &entryData, const QString &sourcePackage);
    ChangelogEntry(const ChangelogEntry &other);
    ~ChangelogEntry();
    ChangelogEntry &operator=(const ChangelogEntry &rhs);

    QString entryText() const;
    QString version() const;
    QDateTime issueDateTime() const;
    QString description() const;

private:
    QT_END_NAMESPACE
    QSharedDataPointer<ChangelogEntryPrivate> d;
    QT_BEGIN_NAMESPACE
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
      * Default constructor
      */
    explicit Changelog(const QString &data, const QString &sourcePackage);
    Changelog(const Changelog &other);
    ~Changelog();
    Changelog &operator=(const Changelog &rhs);

    QString text() const;
    ChangelogEntryList entries() const;
    ChangelogEntryList newEntriesSince(const QString &version) const;

private:
    QT_END_NAMESPACE
    QSharedDataPointer<ChangelogPrivate> d;
    QT_BEGIN_NAMESPACE
};

}

#endif
