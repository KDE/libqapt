/***************************************************************************
 *   Copyright © 2010-2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
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

#include <QObject>
#include <QString>

/**
 * The QApt namespace is the main namespace for LibQApt. All classes in this
 * library fall under this namespace.
 */
namespace QApt {

/**
 * ConfigPrivate is a class containing all private members of the Config class
 */
class ConfigPrivate;

/**
 * @brief Config wrapper for the libapt-pkg config API
 *
 * Config is a wrapper around libapt-pkg's config.h. It provides Qt-style
 * calls to get/set config. It writes to the main apt config file, usually in
 * /etc/apt/apt.conf. It is possible to use this class without a QApt::Backend,
 * but please note that you \b _MUST_ initialize the package system with
 * libapt-pkg before the values returned by readEntry will be accurate.
 *
 * @author Jonathan Thomas
 * @since 1.1
 */
class Q_DECL_EXPORT Config : public QObject
{
    Q_OBJECT
public:
     /**
      * Default constructor
      */
    explicit Config(QObject *parent);

     /**
      * Default destructor
      */
    ~Config();

    /**
     * Reads the value of an entry specified by @p key
     *
     * @param key the key to search for
     * @param defaultValue the default value returned if the key was not found
     *
     * @return the value for this key, or @p default if the key was not found
     *
     * @see writeEntry()
     */
    bool readEntry(const QString &key, const bool defaultValue) const;

    /** Overload for readEntry(const QString&, const bool) */
    int readEntry(const QString &key, const int defaultValue) const;

    /** Overload for readEntry(const QString&, const bool) */
    QString readEntry(const QString &key, const QString &defaultValue) const;

    /**
     * Locates the path of the given key. This uses APT's configuration
     * key algorithm to return various apt-related directories. For example,
     * a key of 'Dir::Etc' would return the location of the APT configuration
     * directory (usually /etc/apt), and 'Dir::Etc::main' would return the
     * location of apt.conf (usually /etc/apt/apt.conf)
     *
     * @param key the key to search for
     * @param defaultValue the directory to use as default if the key isn't found
     *
     * @return the location of the config key, or the default if the key
     * is not found
     * @since 1.4
     */
    QString findDirectory(const QString &key, const QString &defaultValue = QString()) const;

    /**
     * Writes a value to the APT configuration object, and applies it
     *
     * @param key the key to write to
     * @param value the value to write
     *
     * @see readEntry()
     */
    void writeEntry(const QString &key, const bool value);

    /** Overload for writeEntry(const QString&, const bool) */
    void writeEntry(const QString &key, const int value);

    /** Overload for writeEntry(const QString&, const bool) */
    void writeEntry(const QString &key, const QString &value);

    /**
     * Locates the file of the given key. This uses APT's configuration
     * key algorithm to return various apt-related files. For example,
     * a key of 'Dir::Etc::sourcelist' would return the location of the APT
     * sources.list file (usually /etc/apt/sources.list)
     *
     * @param key the key to search for
     * @param defaultValue the directory to use as default if the key isn't found
     *
     * @return the location of the config key, or the default if the key
     * is not found
     * @since 1.4
     */
    QString findFile(const QString &key, const QString &defaultValue = QString()) const;

    /**
     * Returns a list of the CPU architectures APT is configured to support
     *
     * @since 1.4
     */
    QStringList architectures() const;

private:
    Q_DECLARE_PRIVATE(Config)
    ConfigPrivate *const d_ptr;
};

}

#endif
