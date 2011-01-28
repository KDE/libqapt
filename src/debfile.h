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

#ifndef QAPT_DEBFILE_H
#define QAPT_DEBFILE_H

#include <QtCore/QString>

namespace QApt {

/**
 * DebFilePrivate is a class containing all private members of the DebFile class
 */
class DebFilePrivate;

/**
 * The DebFile class is an interface to obtain data from a Debian package
 * archive.
 *
 * @author Jonathan Thomas
 * @since 1.2
 */
class Q_DECL_EXPORT DebFile
{

public:
   /**
    * Default constructor
    */
    DebFile(const QString &filePath);

   /**
    * Default destructor
    */
    ~DebFile();

    bool isValid() const;

   /**
    * Returns the file path of the archive
    *
    * \return The file path of the archive
    */
    QString filePath() const;

   /**
    * Returns the name of the package in the archive
    *
    * \return The name of the package in the archive
    */
    QString packageName() const;

    QLatin1String sourcePackage() const;

    QLatin1String version() const;

    QLatin1String architecture() const;

    QString maintainer() const;

    QLatin1String section() const;

    QLatin1String priority() const;

    QString homepage() const;

    QString shortDescription() const;

    QString longDescription() const;

    QString controlField(const QLatin1String &name) const;

    /** Overload for QString controlField(const QLatin1String &name) const; **/
    QString controlField(const QString &name) const;

    qint64 installedSize() const;

private:
    DebFilePrivate *const d;
};

}

#endif
