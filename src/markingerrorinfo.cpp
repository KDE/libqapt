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

#include "markingerrorinfo.h"

namespace QApt {

class MarkingErrorInfoPrivate : public QSharedData {
public:
    MarkingErrorInfoPrivate()
        : MarkingErrorInfoPrivate(QApt::UnknownReason)
    {}

    MarkingErrorInfoPrivate(BrokenReason reason, const DependencyInfo &info = DependencyInfo())
        : QSharedData()
        , errorType(reason)
        , errorInfo(info)
    {}

    MarkingErrorInfoPrivate(const MarkingErrorInfoPrivate &other)
        : QSharedData(other)
        , errorType(other.errorType)
        , errorInfo(other.errorInfo)
    {}

    // Data members
    QApt::BrokenReason errorType;
    QApt::DependencyInfo errorInfo;
};

MarkingErrorInfo::MarkingErrorInfo()
    : d(new MarkingErrorInfoPrivate())
{
}

MarkingErrorInfo::MarkingErrorInfo(BrokenReason reason, const DependencyInfo &info)
    : d(new MarkingErrorInfoPrivate(reason, info))
{
}

MarkingErrorInfo::MarkingErrorInfo(const MarkingErrorInfo &other)
    : d(other.d)
{
}

MarkingErrorInfo::~MarkingErrorInfo()
{
}

MarkingErrorInfo &MarkingErrorInfo::operator=(const MarkingErrorInfo &rhs)
{
    // Protect against self-assignment
    if (this == &rhs) {
        return *this;
    }
    d = rhs.d;
    return *this;
}

BrokenReason MarkingErrorInfo::errorType() const
{
    return d->errorType;
}

DependencyInfo MarkingErrorInfo::errorInfo() const
{
    return d->errorInfo;
}

}
