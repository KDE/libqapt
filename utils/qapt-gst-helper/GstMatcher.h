/***************************************************************************
 *   Copyright © 2010 Daniel Nicoletti <dantti85-pk@yahoo.com.br>          *
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

#ifndef GSTMATCHER_H
#define GSTMATCHER_H

#include <QtCore/QVector>
#include <QtCore/QStringList>

#include <QGst/Caps>

class PluginInfo;

namespace QApt {
    class Package;
}

class GstMatcher
{
public:
    GstMatcher(const PluginInfo *info);
    ~GstMatcher();

    bool matches(QApt::Package *package);
    bool hasMatches() const;

private:
    const PluginInfo *m_info;
    QVector<QString> m_aptTypes;
};

#endif
