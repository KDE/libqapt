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

#include "installerrunner.h"

// Qt includes
#include <QDir>
#include <QIcon>

// KDE includes
#include <KLocalizedString>
#include <KProcess>
#include <KServiceTypeTrader>

K_EXPORT_PLASMA_RUNNER(installer, InstallerRunner)

InstallerRunner::InstallerRunner(QObject *parent, const QVariantList &args)
    : Plasma::AbstractRunner(parent, args)
{
    Q_UNUSED(args)

    setObjectName("Installation Suggestions");
    setPriority(AbstractRunner::HighestPriority);

    addSyntax(Plasma::RunnerSyntax(":q:", i18n("Suggests the installation of applications if :q: is not found")));
}

InstallerRunner::~InstallerRunner()
{
}

void InstallerRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();
    if (term.length() <  3) {
        return;
    }

    // Search for applications which are executable and case-insensitively match the search term
    // See http://techbase.kde.org/Development/Tutorials/Services/Traders#The_KTrader_Query_Language
    // if the following is unclear to you.
    QString query = QString("exist Exec and ('%1' =~ Name)").arg(term);
    KService::List services = KServiceTypeTrader::self()->query("Application", query);

    QList<Plasma::QueryMatch> matches;

    if (services.isEmpty()) {
        KProcess process;
        if (QFile::exists("/usr/lib/command-not-found")) {
            process << "/usr/lib/command-not-found" << term;
        } else if (QFile::exists("/usr/bin/command-not-found")) {
            process << "/usr/bin/command-not-found" << term;
        } else {
            process << "/bin/ls" << term; // Play it safe even if it won't work at all
        }
        process.setTextModeEnabled(true);
        process.setOutputChannelMode(KProcess::OnlyStderrChannel);
        process.start();
        process.waitForFinished();

        QString output = QString(process.readAllStandardError());
        QStringList resultLines = output.split('\n');
        foreach(const QString &line, resultLines) {
            if (line.startsWith(QLatin1String("sudo"))) {
                QString package = line.split(' ').last();
                Plasma::QueryMatch match(this);
                match.setType(Plasma::QueryMatch::ExactMatch);
                setupMatch(package, term, match);
                match.setRelevance(1);
                matches << match;
            }
        }
    }

    if (!context.isValid()) {
        return;
    }


    context.addMatches(matches);
}

void InstallerRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context);
    QString package = match.data().toString();
    if (!package.isEmpty()) {
        KProcess *process = new KProcess(this);
        QStringList args = QStringList() << "--install" << package;
        process->setProgram("/usr/bin/qapt-batch", args);
        process->start();
    }
}

void InstallerRunner::setupMatch(const QString &package, const QString &term, Plasma::QueryMatch &match)
{
    match.setText(i18n("Install %1", package));
    match.setData(package);
    if (term != package) {
        match.setSubtext(i18n("The \"%1\" package contains %2", package, term));
    }

    match.setIcon(QIcon::fromTheme("applications-other"));
}

#include "installerrunner.moc"
