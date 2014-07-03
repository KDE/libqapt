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

#include "PluginFinder.h"

#include <QThread>

#include <QApt/Backend>

#include "GstMatcher.h"
#include "PluginInfo.h"

PluginFinder::PluginFinder(QObject *parent, QApt::Backend *backend)
    : QObject(parent)
    , m_backend(backend)
    , m_stop(false)
{
}

PluginFinder::~PluginFinder()
{
}

void PluginFinder::find(const PluginInfo *pluginInfo)
{
    if (m_stop) {
        return;
    }

    GstMatcher matcher(pluginInfo);

    if (!matcher.hasMatches()) {
        // No such codec
        emit notFound();
        return;
    }

    foreach (QApt::Package *package, m_backend->availablePackages()) {
        if (matcher.matches(package) && package->architecture() == m_backend->nativeArchitecture()) {
            emit foundCodec(package);
            return;
        }
    }

    emit notFound();
}

void PluginFinder::setSearchList(const QList<PluginInfo *> &list)
{
    m_searchList = list;
}

void PluginFinder::startSearch()
{
    foreach(PluginInfo *info, m_searchList) {
        find(info);
    }

    thread()->quit();
}

void PluginFinder::stop()
{
    m_stop = true;
}
