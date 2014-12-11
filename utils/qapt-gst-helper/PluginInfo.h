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

#ifndef PLUGININFO_H
#define PLUGININFO_H

#include <QString>

// Apt and Gst both define FLAG, forcefully undef the Apt def and hope nothing
// breaks :/
#undef FLAG
#include <gst/gst.h>

class PluginInfo
{
public:
    enum PluginType {
        InvalidType = 0,
        Encoder,
        Decoder,
        UriSource,
        UriSink,
        Element
    };

    explicit PluginInfo(const QString &gstDetails = QString());
    ~PluginInfo();

    QString version() const;
    QString requestedBy() const;
    QString name() const;
    QString capsInfo() const;
    int pluginType() const;
    QString data() const;
    QString typeName() const;

    bool isValid() const;

private:
    QString m_version;
    QString m_requestedBy;
    QString m_name;
    QString m_typeName;
    QString m_capsInfo;
    GstStructure *m_structure;
    int m_pluginType;
    QString m_data;

    bool m_isValid;

    void parseDetails(const QString &gstDetails);
};

#endif
