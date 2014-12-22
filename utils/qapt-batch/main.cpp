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

#include "qaptbatch.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QIcon>
#include <QPointer>

#include <KAboutData>
#include <KLocalizedString>

static const char description[] =
    I18N_NOOP2("@info", "A batch installer using QApt");

static const char version[] = CMAKE_PROJECT_VERSION;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme("applications-other"));

    KLocalizedString::setApplicationDomain("qapt-batch");

    KAboutData aboutData("qaptbatch",
                         i18nc("@title", "QApt Batch Installer"),
                         version,
                         i18nc("@info", description),
                         KAboutLicense::LicenseKey::GPL,
                         i18nc("@info:credit", "(C) 2010 Jonathan Thomas"));

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
    QCommandLineOption attachOption(QStringLiteral("attach"),
                                    i18nc("@info:shell", "Attaches the window to an X app specified by winid"),
                                    i18nc("@info:shell value name", "winid"),
                                    QStringLiteral("0"));
    parser.addOption(attachOption);
    QCommandLineOption installOption(QStringLiteral("install"),
                                    i18nc("@info:shell", "Install a package"));
    parser.addOption(installOption);
    QCommandLineOption uninstallOption(QStringLiteral("uninstall"),
                                    i18nc("@info:shell", "Remove a package"));
    parser.addOption(uninstallOption);
    QCommandLineOption updateOption(QStringLiteral("update"),
                                    i18nc("@info:shell", "Update the package cache"));
    parser.addOption(updateOption);
    parser.addPositionalArgument("packages",
                                 i18nc("@info:shell", "Packages to be operated upon"));
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    QString mode;

    int winId = parser.value(attachOption).toInt();

    if (!(parser.isSet(installOption) ^ parser.isSet(uninstallOption) ^ parser.isSet(updateOption))) {
        qCritical() << i18nc("@info:shell error", "Only one operation mode may be defined.");
        return 1;
    }

    if (parser.isSet(installOption)) {
        mode = QStringLiteral("install");
    } else if (parser.isSet(uninstallOption)) {
        mode = QStringLiteral("uninstall");
    } else if (parser.isSet(updateOption)) {
        mode = QStringLiteral("update");
    } else {
        qCritical() << i18nc("@info:shell error", "No operation mode defined.");
        return 1;
    }

    QStringList packages = parser.positionalArguments();

    Q_UNUSED(app);
    QPointer<QAptBatch> batchInstaller = new QAptBatch(mode, packages, winId);
    switch (batchInstaller->exec()) {
        case QDialog::Accepted:
            return 0;
            break;
        case QDialog::Rejected:
        default:
            return 1;
    }
    app.exec();
}
