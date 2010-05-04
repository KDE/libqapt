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

#ifndef WORKERINSTALLPROGRESS_H
#define WORKERINSTALLPROGRESS_H

#include <QtCore/QVariantMap>

#include <apt-pkg/packagemanager.h>

class WorkerInstallProgress : public QObject
{
    Q_OBJECT
public:
    WorkerInstallProgress(QObject* parent);
    ~WorkerInstallProgress();

    pkgPackageManager::OrderResult start(pkgPackageManager *pm);

private:
    int m_stdout;
    int m_stderr;
    int m_childin;

    pid_t m_child_id;
    bool m_startCounting;

    void updateInterface(int fd);

signals:
    void commitProgress(QString status, int percentage);
    void commitError(int code, const QVariantMap &details);
};

#endif
