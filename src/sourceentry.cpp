/***************************************************************************
 *   Copyright © 2013 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#include "sourceentry.h"

// Qt includes
#include <QSharedData>
#include <QStringList>

namespace QApt {

class SourceEntryPrivate : public QSharedData {
public:
    SourceEntryPrivate()
        : isValid(true)
        , isEnabled(true)
    {}

    // Data members
    bool isValid;
    bool isEnabled;
    QString type;
    QStringList architectures;
    QString uri;
    QString dist;
    QStringList components;
    QString comment;
    QString line;
    QString file;
};

SourceEntry::SourceEntry()
    : d(new SourceEntryPrivate)
{
}

SourceEntry::SourceEntry(const SourceEntry &rhs)
    : d(rhs.d)
{
}

SourceEntry &SourceEntry::operator=(const SourceEntry &rhs)
{
    if (this != &rhs)
        d.operator=(rhs.d);
    return *this;
}

SourceEntry::~SourceEntry()
{
}

}
