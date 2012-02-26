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

#include "GstMatcher.h"

#include <KDebug>

#include <../../src/package.h>

#include "PluginInfo.h"

GstMatcher::GstMatcher(const PluginInfo *info)
{
    m_aptTypes = QVector<QString>(6);
    m_aptTypes[PluginInfo::InvalidType] = QLatin1String("");
    m_aptTypes[PluginInfo::Encoder] = QLatin1String("Gstreamer-Encoders");
    m_aptTypes[PluginInfo::Decoder] = QLatin1String("Gstreamer-Decoders");
    m_aptTypes[PluginInfo::UriSource] = QLatin1String("Gstreamer-Uri-Sources");
    m_aptTypes[PluginInfo::UriSink] = QLatin1String("Gstreamer-Uri-Sinks");
    m_aptTypes[PluginInfo::Element] = QLatin1String("Gstreamer-Elements");

    m_info = info;
}

GstMatcher::~GstMatcher()
{
}

bool GstMatcher::matches(QApt::Package *package)
{
    if (package->controlField(QLatin1String("Gstreamer-Version")) != m_info->version())
        return false;

    QString typeName = m_aptTypes[m_info->pluginType()];
    QString typeData = package->controlField(typeName);

    if (typeData.isEmpty())
        return false;

    QGst::CapsPtr packageCaps = QGst::Caps::fromString(typeData);
    QGst::CapsPtr pluginCaps = QGst::Caps::fromString(m_info->capsInfo());

    switch (m_info->pluginType()) {
    case PluginInfo::Encoder:
    case PluginInfo::Decoder:
        if (!packageCaps || !pluginCaps || packageCaps->isEmpty() || pluginCaps->isEmpty())
            return false;

        return pluginCaps->canIntersect(packageCaps);
    case PluginInfo::Element:
    case PluginInfo::UriSink:
    case PluginInfo::UriSource:
        return typeData.contains(m_info->capsInfo());
    default:
        break;
    }

    return false;
}

bool GstMatcher::hasMatches() const
{
    return m_info->isValid();
}

