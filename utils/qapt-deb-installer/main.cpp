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

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>

static const char description[] =
    I18N_NOOP2("@info", "A Debian package installer");

static const char version[] = "1.4";

int main(int argc, char **argv)
{
    KAboutData about("qapt-deb-installer", 0, ki18nc("@title", "QApt Package Installer"), version, ki18nc("@info", description),
                     KAboutData::License_GPL, ki18nc("@info:credit", "(C) 2011 Jonathan Thomas"), KLocalizedString(), 0, "echidnaman@kubuntu.org");
    about.addAuthor( ki18nc("@info:credit", "Jonathan Thomas"), KLocalizedString(), "echidnaman@kubuntu.org" );
    about.setProgramIconName("applications-other");
    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;
    options.add("+[File]", ki18nc("@info:shell", ".deb file"));
    KCmdLineArgs::addCmdLineOptions(options);

    KApplication app;

    // do not restore!
    if (app.isSessionRestored()) {
        exit(0);
    }

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    QString debFile;

    if (args->count()) {
        debFile = args->arg(0);
    }

    DebInstaller debInstaller(0, debFile);

    switch (debInstaller.exec()) {
        case QDialog::Accepted:
            return 0;
            break;
        case QDialog::Rejected:
        default:
            return 1;
    }

    return 0;
}
