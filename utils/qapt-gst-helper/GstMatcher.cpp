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

#include <QDebug>

#include <QApt/Package>

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
    // Reject already-installed packages
    if (package->isInstalled())
        return false;

    // There is a bug in Ubuntu (and supposedly Debian) where it lists an incorrect
    // version, see below. To work around the problem a more fuzzy match is used,
    // to force strict matching, use export QAPT_GST_STRICT_VERSION_MATCH=1.
    if (!qgetenv("QAPT_GST_STRICT_VERSION_MATCH").isEmpty()) {
        if (package->controlField(QLatin1String("Gstreamer-Version")) != m_info->version())
            return false;
    } else {
        // Excitingly silly code following...

        QString packageVersion = package->controlField(QLatin1String("Gstreamer-Version"));

        if (packageVersion.isEmpty()) // No version, discard.
            return false;

        QStringList packageVersionFields = packageVersion.split(QChar('.'));
        if (packageVersionFields.size() != 2) // must be x.y or we don't consider it a valid API version number.
            return false;

        QStringList infoVersionFields = m_info->version().split(QChar('.'));

        // x and y in x.y: both of the package need to be greater or equal to the ones
        // in the request.
        // WARNING: This is a bloody workaround for Ubuntu having broken versions.
        //          In particular Ubuntu thinks that gst_version(...) is the same
        //          as the GST_API_VERSION, which worked out fine for 0.1x but fails
        //          for 1.x. For example at the time of writing api version is at 1.0
        //          but gst_version is at 1.2. libgstreamer will however continue to
        //          ask for api version, so we get a request for 1.0 but the packages
        //          say they are 1.2 .... -.-
        for (int i = 0; i < 2; ++i) {
            int packageVersion = packageVersionFields.at(i).toInt();
            int infoVersion = infoVersionFields.at(i).toInt();
            if (i == 0) {
                if (packageVersion != infoVersion)
                    return false;
            } else {
                if (packageVersion < infoVersion)
                    return false;
            }
        }

        // At this point we have assured that the package has a version, that it is
        // of the form x.y and that the package's x.y is >= the request's x.y.
        // We now consider the version an acceptable match.

        // End of excitingly silly code.
    }

    QString typeName = m_aptTypes[m_info->pluginType()];
    QString typeData = package->controlField(typeName);

    if (typeData.isEmpty())
        return false;

    // We are handling gobjects that need cleanup, so we'll do a delayed return.
    bool ret = false;

    GstCaps *packageCaps = gst_caps_from_string(typeData.toUtf8().constData());
    GstCaps *pluginCaps = gst_caps_from_string(m_info->capsInfo().toUtf8().constData());

    switch (m_info->pluginType()) {
    case PluginInfo::Encoder:
    case PluginInfo::Decoder:
        if (!packageCaps || !pluginCaps || gst_caps_is_empty(packageCaps) || gst_caps_is_empty(pluginCaps))
            ret = false;

        ret = gst_caps_can_intersect(pluginCaps, packageCaps);
        break;
    case PluginInfo::Element:
    case PluginInfo::UriSink:
    case PluginInfo::UriSource:
        ret = typeData.contains(m_info->capsInfo());
        break;
    }

    gst_caps_unref(pluginCaps);
    gst_caps_unref(packageCaps);
    return ret;
}

bool GstMatcher::hasMatches() const
{
    return m_info->isValid();
}
