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

#ifndef QAPT_HISTORY_H
#define QAPT_HISTORY_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSharedDataPointer>
#include <QString>

#include "package.h"

/**
 * The QApt namespace is the main namespace for LibQApt. All classes in this
 * library fall under this namespace.
 */
namespace QApt {

/**
 * HistoryItemPrivate is a class containing all private members of the HistoryItem class
 */
class HistoryItemPrivate;

/**
 * The HistoryItem class represents a single entry in the APT history logs
 *
 * @author Jonathan Thomas
 * @since 1.1
 */
class Q_DECL_EXPORT HistoryItem
{
public:
   /**
    * Constructor
    *
    * @param data The raw text from the history log
    */
    HistoryItem(const QString &data);

   /**
    * Shallow copy constructor
    *
    * @param other The HistoryItem to be copied
    */
    HistoryItem(const HistoryItem &other);

    /**
     * Constructor
     */
    ~HistoryItem();

    /**
     * Equality operator implementing a shallow copy
     *
     * @param rhs The object to be copied
     */
    HistoryItem &operator=(const HistoryItem &rhs);

   /**
    * Returns the date upon which the transaction occurred.
    *
    * @return the date when the transaction occurred as a \c QDateTime
    */
    QDateTime startDate() const;

   /**
    * Returns the list of packages installed during the transaction
    *
    * @return the package list as a \c QStringList
    */
    QStringList installedPackages() const;

   /**
    * Returns the list of packages upgraded during the transaction
    *
    * @return the package list as a \c QStringList
    */
    QStringList upgradedPackages() const;

   /**
    * Returns the list of packages downgraded during the transaction
    *
    * @return the package list as a \c QStringList
    */
    QStringList downgradedPackages() const;

   /**
    * Returns the list of packages removed during the transaction
    *
    * @return the package list as a \c QStringList
    */
    QStringList removedPackages() const;

   /**
    * Returns the list of packages purged during the transaction
    *
    * @return the package list as a \c QStringList
    */
    QStringList purgedPackages() const;

   /**
    * Returns the error reported by dpkg, if there is one. If the transaction
    * did not encounter an error, this will return an empty QString.
    *
    * @return the error as a \c QString
    */
    QString errorString() const;

   /**
    * Returns whether or not the HistoryItem is valid. (It could be an improperly
    * parsed portion of the log
    */
    bool isValid() const;

private:
    QSharedDataPointer<HistoryItemPrivate> d;
};

typedef QList<HistoryItem> HistoryItemList;

/**
 * HistoryPrivate is a class containing all private members of the History class
 */
class HistoryPrivate;


/**
 * The History class is an interface for retreiving information from the APT
 * history logs.
 *
 * @author Jonathan Thomas
 * @since 1.1
 */
class Q_DECL_EXPORT History : public QObject
{
    Q_OBJECT
public:
    /**
     * Constructor
     */
    explicit History(QObject *parent);

    /**
     * Destructor
     */
    ~History();

    /**
     * Returns a list of all history items in APT's history logs.
     */
    HistoryItemList historyItems() const;

private:
    Q_DECLARE_PRIVATE(History)
    HistoryPrivate *const d_ptr;

public Q_SLOTS:
   /**
    * Re-initializes the history log data. This should be connected to a
    * directory watch (such as KDirWatch) to catch changes to the history
    * file on-the-fly, if desired
    */
    void reload();
};

}

Q_DECLARE_TYPEINFO(QApt::HistoryItem, Q_MOVABLE_TYPE);

#endif
