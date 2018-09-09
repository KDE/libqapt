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

#ifndef MARKINGERRORINFO_H
#define MARKINGERRORINFO_H

#include <QSharedDataPointer>
#include <QString>

#include "dependencyinfo.h"

namespace QApt {

class MarkingErrorInfoPrivate;

class Q_DECL_EXPORT MarkingErrorInfo
{
public:
    /**
     * Default constructor, empty values
     */
    explicit MarkingErrorInfo();

    /**
     * Constructs a marking error information object with the given
     * reason and dependency info
     *
     * @param reason Why a marking is broken
     * @param info Additional dependency information describing the error
     */
    explicit MarkingErrorInfo(QApt::BrokenReason reason, const DependencyInfo &info = DependencyInfo());

    /**
     * Copy constructor
     */
    explicit MarkingErrorInfo(const MarkingErrorInfo &other);

    /**
     * Default Destructor
     */
    ~MarkingErrorInfo();

    /**
     * Assignment operator
     */
    MarkingErrorInfo &operator=(const MarkingErrorInfo &rhs);

    QApt::BrokenReason errorType() const;

    /**
     * Returns dependency info related to the error, if the error
     * is dependency-related. (E.g. info on an uninstallable parent,
     * info on an uninstallable dependency, or info about the wrong
     * version of a package being available for installation.
     */
    QApt::DependencyInfo errorInfo() const;

private:
    QSharedDataPointer<MarkingErrorInfoPrivate> d;
};

}

Q_DECLARE_TYPEINFO(QApt::MarkingErrorInfo, Q_MOVABLE_TYPE);

#endif // MARKINGERRORINFO_H
