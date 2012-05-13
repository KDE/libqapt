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

#include "PluginHelper.h"

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>

#include <QGst/Init>

static const char description[] =
    I18N_NOOP2("@info", "A GStreamer codec installer using QApt");

static const char version[] = "1.4";

int main(int argc, char **argv)
{
    KAboutData about("qapt-gst-helper", 0, ki18nc("@title", "QApt Codec Searcher"), version, ki18nc("@info", description),
                     KAboutData::License_GPL, ki18nc("@info:credit", "(C) 2011 Jonathan Thomas"), KLocalizedString(), 0, "echidnaman@kubuntu.org");
    about.addAuthor( ki18nc("@info:credit", "Jonathan Thomas"), KLocalizedString(), "echidnaman@kubuntu.org" );
    about.setProgramIconName("applications-other");
    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;
    options.add("transient-for <winid>", ki18nc("@info:shell","Attaches the window to an X app specified by winid"));
    options.add("+[GStreamer Info]", ki18nc("@info:shell", "GStreamer install info"));
    KCmdLineArgs::addCmdLineOptions(options);

    KApplication app;
    QGst::init(&argc, &argv);

    // do not restore!
    if (app.isSessionRestored()) {
        exit(0);
    }

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    int winId;
    if (args->isSet("transient-for") && args->count() > 0) {
        winId = args->getOption("transient-for").toInt();
    } else {
        winId = 0;
    }

    QStringList details;

    for(int i = 0; i < args->count(); i++) { // Counting start at 0!
        details << args->arg(i);
    }

    PluginHelper pluginHelper(0, details, winId);
    pluginHelper.run();

    return app.exec();
}
