/*
 * Copyright (C) 2007 Sebastian Trueg <trueg@kde.org>
 * Copyright (C) 2009 Dario Freddi <drf@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _AQPM_VERSION_H_
#define _AQPM_VERSION_H_

#include "Visibility.h"

/// @brief QApt version as string at compile time.
#define AQPM_VERSION_STRING "${AQPM_VERSION_STRING}"

/// @brief The major QApt version number at compile time
#define AQPM_VERSION_MAJOR ${MAJOR_AQPM_VERSION}

/// @brief The minor QApt version number at compile time
#define AQPM_VERSION_MINOR ${MINOR_AQPM_VERSION}

/// @brief The QApt release version number at compile time
#define AQPM_VERSION_RELEASE ${PATCH_AQPM_VERSION}

/// @brief The QApt fix version number at compile time
#define AQPM_VERSION_FIX ${FIX_AQPM_VERSION}

/**
 * \brief Create a unique number from the major, minor and release number of a %QApt version
 *
 * This function can be used for preprocessing. For version information at runtime
 * use the version methods in the QApt namespace.
 */
#define AQPM_MAKE_VERSION( a,b,c,d ) (((a) << 16) | ((b) << 8) | (c) | (d))

/**
 * \brief %QApt Version as a unique number at compile time
 *
 * This macro calculates the %QApt version into a number. It is mainly used
 * through AQPM_IS_VERSION in preprocessing. For version information at runtime
 * use the version methods in the QApt namespace.
 */
#define AQPM_VERSION \
    AQPM_MAKE_VERSION(AQPM_VERSION_MAJOR,AQPM_VERSION_MINOR,AQPM_VERSION_RELEASE,AQPM_VERSION_FIX)

/**
 * \brief Check if the %QApt version matches a certain version or is higher
 *
 * This macro is typically used to compile conditionally a part of code:
 * \code
 * #if AQPM_IS_VERSION(2,1)
 * // Code for QApt 2.1
 * #else
 * // Code for QApt 2.0
 * #endif
 * \endcode
 *
 * For version information at runtime
 * use the version methods in the QApt namespace.
 */
#define AQPM_IS_VERSION(a,b,c,d) ( AQPM_VERSION >= AQPM_MAKE_VERSION(a,b,c,d) )


namespace QApt {
    /**
     * @brief Returns the major number of QApt's version, e.g.
     * 1 for %QApt 1.0.2.
     * @return the major version number at runtime.
     */
    AQPM_EXPORT unsigned int versionMajor();

    /**
     * @brief Returns the minor number of QApt's version, e.g.
     * 0 for %QApt 1.0.2.
     * @return the minor version number at runtime.
     */
    AQPM_EXPORT unsigned int versionMinor();

    /**
     * @brief Returns the release of QApt's version, e.g.
     * 2 for %QApt 1.0.2.
     * @return the release number at runtime.
     */
    AQPM_EXPORT unsigned int versionRelease();

    /**
     * @brief Returns the release of QApt's version, e.g.
     * 3 for %QApt 1.0.2.3.
     * @return the release number at runtime.
     */
    AQPM_EXPORT unsigned int versionFix();

    /**
     * @brief Returns the %QApt version as string, e.g. "1.0.2".
     *
     * On contrary to the macro AQPM_VERSION_STRING this function returns
     * the version number of QApt at runtime.
     * @return the %QApt version. You can keep the string forever
     */
    AQPM_EXPORT const char* versionString();
}

#endif
