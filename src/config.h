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

#ifndef QAPT_CONFIG_H
#define QAPT_CONFIG_H

#include <QtCore/QString>

/**
 * The QApt namespace is the main namespace for LibQApt. All classes in this
 * library fall under this namespace.
 */
namespace QApt {

/**
 * @brief Config wrapper for the libapt-pkg config API
 *
 * Config is a wrapper around libapt-pkg's config.h. It provides Qt-style
 * calls to get/set config. It writes to the main apt config file, usually in
 * \/etc/apt/apt.conf.
 *
 * @author Jonathan Thomas
 */
class Config
{
public:
     /**
      * Default constructor
      */
    explicit Config();

     /**
      * Default destructor
      */
    virtual ~Config();

    bool readEntry(const QString &key, const bool defaultValue);
    int readEntry(const QString &key, const int defaultValue);
    QString readEntry(const QString &key, const QString &defaultValue);
};

}

#endif
