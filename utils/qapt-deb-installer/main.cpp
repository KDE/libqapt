/***************************************************************************
 *   Copyright © 2011,2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
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

#include "DebInstaller.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QPointer>

#include <KAboutData>
#include <KLocalizedString>

static const char description[] =
    I18N_NOOP2("@info", "A Debian package installer");

static const char version[] = CMAKE_PROJECT_VERSION;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme("applications-other"));

    KLocalizedString::setApplicationDomain("qapt-deb-installer");

    KAboutData aboutData("qapt-deb-installer",
                         i18nc("@title", "QApt Package Installer"),
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
    parser.addPositionalArgument("file",
                                 i18nc("@info:shell argument", ".deb file"));
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    // do not restore!
    if (app.isSessionRestored()) {
        exit(0);
    }

    QString debFile;

    if (parser.positionalArguments().size() > 0) {
        debFile = parser.positionalArguments().at(0);
    }

    QPointer<DebInstaller> debInstaller = new DebInstaller(0, debFile);

    switch (debInstaller->exec()) {
        case QDialog::Accepted:
            return 0;
            break;
        case QDialog::Rejected:
        default:
            return 1;
    }

    return 0;
}
