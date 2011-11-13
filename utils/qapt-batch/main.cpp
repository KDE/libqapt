/***************************************************************************
 *   Copyright © 2010 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#include "qaptbatch.h"
#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>

static const char description[] =
    I18N_NOOP2("@info", "A batch installer using QApt");

static const char version[] = "1.3";

int main(int argc, char **argv)
{
    KAboutData about("qaptbatch", 0, ki18nc("@title", "QApt Batch Installer"), version, ki18nc("@info", description),
                     KAboutData::License_GPL, ki18nc("@info:credit", "(C) 2010 Jonathan Thomas"), KLocalizedString(), 0, "echidnaman@kubuntu.org");
    about.addAuthor( ki18nc("@info:credit", "Jonathan Thomas"), KLocalizedString(), "echidnaman@kubuntu.org" );
    about.setProgramIconName("applications-other");
    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;
    options.add("attach <winid>", ki18nc("@info:shell","Attaches the window to an X app specified by winid"));
    options.add("install", ki18nc("@info:shell", "Install a package"));
    options.add("uninstall", ki18nc("@info:shell", "Remove a package"));
    options.add("update", ki18nc("@info:shell", "Update the package cache"));
    options.add("+[Package(s)]", ki18nc("@info:shell", "Packages to be operated upon"));
    KCmdLineArgs::addCmdLineOptions(options);

    int winId;
    QString mode;

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    if (args->isSet("attach") && args->count() > 0) {
        winId = args->getOption("attach").toInt();
    } else {
        winId = 0;
    }

    if (args->isSet("install") && args->count() > 0) {
        mode = QString("install");
    } else if (args->isSet("uninstall") && args->count() > 0) {
        mode = QString("uninstall");
    } else if (args->isSet("update")) {
        mode = QString("update");
    } else {
        return 1;
    }

    QStringList packages;

    for(int i = 0; i < args->count(); i++) { // Counting start at 0!
        packages << args->arg(i);
    }

    KApplication app;
    QAptBatch batchInstaller(mode, packages, winId);
    switch (batchInstaller.exec()) {
        case QDialog::Accepted:
            return 0;
            break;
        case QDialog::Rejected:
        default:
            return 1;
    }
}
