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

#include "PluginInfo.h"

#include <QtCore/QStringBuilder>
#include <QtCore/QStringList>
#include <QDebug>

#include <glib-object.h>

PluginInfo::PluginInfo(const QString &gstDetails)
          : m_pluginType(InvalidType)
          , m_data(gstDetails)
          , m_isValid(true)
{
    parseDetails(gstDetails);
}

PluginInfo::~PluginInfo()
{
}

void PluginInfo::parseDetails(const QString &gstDetails)
{
    QStringList parts = gstDetails.split('|');

    if (parts.count() != 5) {
        m_isValid = false;
        return;
    }

    m_version = parts.at(1);
    m_requestedBy = parts.at(2);
    m_name = parts.at(3);
    m_capsInfo = parts.at(4);

    if (m_capsInfo.contains("encoder")) {
        m_pluginType = Encoder;
    } else if (m_capsInfo.contains("decoder")) {
        m_pluginType = Decoder;
    }

    gchar **split = NULL;
    gchar **ss = NULL;
    GstStructure *s;
    gchar *caps = NULL;

    split = g_strsplit (gstDetails.toStdString().c_str(), "|", -1);

    if (m_capsInfo.startsWith("uri")) {
        /* split uri */
        ss = g_strsplit (split[4], " ", 2);

        m_typeName = g_strdup(ss[0]);
        goto out;
    }

    /* split */
    gst_init(NULL, NULL);
    ss = g_strsplit (split[4], "-", 2);
    m_typeName = g_strdup (ss[0]);
    caps = g_strdup (ss[1]);

    s = gst_structure_from_string (caps, NULL);
    if (s == NULL) {
        g_message ("Failed to parse caps: %s", caps);
        goto out;
    }

    /* remove fields that are almost always just MIN-MAX of some sort
     * in order to make the caps look less messy */
    gst_structure_remove_field (s, "pixel-aspect-ratio");
    gst_structure_remove_field (s, "framerate");
    gst_structure_remove_field (s, "channels");
    gst_structure_remove_field (s, "width");
    gst_structure_remove_field (s, "height");
    gst_structure_remove_field (s, "rate");
    gst_structure_remove_field (s, "depth");
    gst_structure_remove_field (s, "clock-rate");
    gst_structure_remove_field (s, "bitrate");

    m_structure = s;

out:
    g_free (caps);
    g_strfreev (ss);
    g_strfreev (split);
}

QString PluginInfo::version() const
{
    return m_version;
}

QString PluginInfo::requestedBy() const
{
    return m_requestedBy;
}

QString PluginInfo::name() const
{
    return m_name;
}

QString PluginInfo::capsInfo() const
{
    return m_capsInfo;
}

int PluginInfo::pluginType() const
{
    return m_pluginType;
}

QString PluginInfo::data() const
{
    return m_data;
}

QString PluginInfo::typeName() const
{
    return m_typeName;
}

QString PluginInfo::searchString() const
{
    QString searchString = QLatin1Literal("gstreamer") % m_version % '(';
    if (m_structure) {
        searchString.append(m_typeName % '-' % gst_structure_get_name(m_structure) % ')');
    } else {
        searchString.append(m_capsInfo % ')');
    }

    return searchString;
}

bool PluginInfo::isValid() const
{
    return m_isValid;
}
