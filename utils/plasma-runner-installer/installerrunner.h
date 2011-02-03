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

#ifndef INSTALLERRUNNER_H
#define INSTALLERRUNNER_H

#include <Plasma/AbstractRunner>

/**
 * This runner checks if the query exists as an executable in the normal paths
 * and suggests the installation of the package that would normally contain
 * the executable if not found.
 */
class InstallerRunner : public Plasma::AbstractRunner
{
Q_OBJECT

public:
    InstallerRunner(QObject *parent, const QVariantList &args);
    ~InstallerRunner();

    void match(Plasma::RunnerContext &context);
    void run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &action);

protected:
    void setupMatch(const QString &package, const QString &term, Plasma::QueryMatch &action);
};

K_EXPORT_PLASMA_RUNNER(installer, InstallerRunner)

#endif

