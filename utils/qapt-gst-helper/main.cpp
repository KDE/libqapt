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

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

#include <KAboutData>
#include <KLocalizedString>

#include <gst/gst.h>
#include <gst/pbutils/install-plugins.h>

static const char description[] =
    I18N_NOOP2("@info", "A GStreamer codec installer using QApt");

static const char version[] = CMAKE_PROJECT_VERSION;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme("applications-other"));

    KLocalizedString::setApplicationDomain("qapt-gst-helper");

    KAboutData aboutData("qapt-gst-helper",
                         i18nc("@title", "QApt Codec Searcher"),
                         version,
                         i18nc("@info", description),
                         KAboutLicense::LicenseKey::GPL,
                         i18nc("@info:credit", "(C) 2011 Jonathan Thomas"));

    aboutData.addAuthor(i18nc("@info:credit", "Jonathan Thomas"),
                        QString(),
                        QStringLiteral("echidnaman@kubuntu.org"));
    aboutData.addAuthor(i18nc("@info:credit", "Harald Sitter"),
                        i18nc("@info:credit", "Qt 5 port"),
                        QStringLiteral("apachelogger@kubuntu.org"));
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption transientOption(QStringLiteral("transient-for"),
                                       i18nc("@info:shell", "Attaches the window to an X app specified by winid"),
                                       i18nc("@info:shell value name", "winid"),
                                       QStringLiteral("0"));
    parser.addOption(transientOption);
    parser.addPositionalArgument("GStreamer Info",
                                 i18nc("@info:shell", "GStreamer install info"));
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    GError *error = nullptr;
    gst_init_check(&argc, &argv, &error);
    if (error) {
        // TODO: we should probably show an error message. API documention suggests
        // so at least. Then again explaining random init errors to the user
        // might be a bit tricky.
#warning FIXME 3.1 show error msgbox when gstreamer init fails
        return GST_INSTALL_PLUGINS_ERROR;
    }

    // do not restore!
    if (app.isSessionRestored()) {
        exit(0);
    }

    int winId = parser.value(transientOption).toInt();
    QStringList details = parser.positionalArguments();

    PluginHelper pluginHelper(0, details, winId);
    pluginHelper.run();

    return app.exec();
}
