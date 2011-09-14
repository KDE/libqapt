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

#include <QStringBuilder>

#include <KDebug>

#include <regex.h>

#include <../../src/package.h>

GstMatcher::GstMatcher(const QStringList &values)
{
    // The search term from PackageKit daemon:
    // gstreamer0.10(urisource-foobar)
    // gstreamer0.10(decoder-audio/x-wma)(wmaversion=3)
    const char *pkreg = "^gstreamer\\([0-9\\.]\\+\\)"
                "(\\(encoder\\|decoder\\|urisource\\|urisink\\|element\\)-\\([^)]\\+\\))"
                "\\(([^\\(^\\)]*)\\)\\?";

    regex_t pkre;
    if(regcomp(&pkre, pkreg, 0) != 0) {
        return;
    }

    kDebug() << values;

    for (int i = 0; i < values.size(); i++) {
        string value = values.at(i).toStdString();
        regmatch_t matches[5];
        if (regexec(&pkre, value.c_str(), 5, matches, 0) == 0) {
            Match values;
            string version, type, data, opt;

            // Appends the version
            version = string(value.c_str(), matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);

            // type (encode|decoder...)
            type = string(value.c_str(), matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);

            // data "audio/x-wma"
            data = string(value.c_str(), matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);

            // opt "wmaversion=3"
            if (matches[4].rm_so != -1) {
                // remove the '(' ')' that the regex matched
                opt = string(value.c_str(), matches[4].rm_so + 1, matches[4].rm_eo - matches[4].rm_so - 2);
                kDebug() << QString::fromStdString(opt);
            }

            if (type.compare("encoder") == 0) {
                type = "Gstreamer-Encoders";
            } else if (type.compare("decoder") == 0) {
                type = "Gstreamer-Decoders";
            } else if (type.compare("urisource") == 0) {
                type = "Gstreamer-Uri-Sources";
            } else if (type.compare("urisink") == 0) {
                type = "Gstreamer-Uri-Sinks";
            } else if (type.compare("element") == 0) {
                type = "Gstreamer-Elements";
            }

            QString capsString;
            if (opt.empty()) {
                capsString = QLatin1String(data.c_str());
            } else {
                capsString = QLatin1String(data.c_str()) % QLatin1Literal(", ")
                             % QLatin1String(opt.c_str());
            }

            QGst::CapsPtr caps = QGst::Caps::fromString(capsString);
            if (!caps || caps->isEmpty()) {
                continue;
            }

            values.version = version;
            values.type    = type;
            values.data    = data;
            values.opt     = opt;
            values.caps    = caps;

            m_matches.append(values);
        } else {
            kDebug() << "Did not match: ";
        }
    }
    regfree(&pkre);
}

GstMatcher::~GstMatcher()
{
}

bool GstMatcher::matches(QApt::Package *package)
{
    for (QVector<Match>::const_iterator i = m_matches.constBegin(); i != m_matches.constEnd(); ++i) {
            // Tries to find "Gstreamer-version: xxx"
            if (package->controlField(QLatin1String("Gstreamer-Version")) == QString::fromStdString(i->version)) {
                // Tries to find the type (e.g. "Gstreamer-Uri-Sinks: ")
                QString typeData = package->controlField(QLatin1String(i->type.c_str()));
                if (!typeData.isEmpty()) {

                    QGst::CapsPtr caps = QGst::Caps::fromString(typeData);
                    if (caps->isEmpty()) {
                        continue;
                    }

                    // If the package's cap intersects with our cap from the
                    // search string, return true
                    return (i->caps)->canIntersect(caps);
                }
            }
        }
        return false;
}

bool GstMatcher::hasMatches() const
{
    return !m_matches.isEmpty();
}

