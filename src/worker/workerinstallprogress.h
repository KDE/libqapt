/***************************************************************************
 *   Copyright © 2010-2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
 *   Copyright © 2004 Canonical <mvo@debian.org>                           *
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

#include <apt-pkg/packagemanager.h>

class Transaction;

class WorkerInstallProgress
{
public:
    explicit WorkerInstallProgress(int begin = 0, int end = 100);

    void setTransaction(Transaction *trans);
    pkgPackageManager::OrderResult start(pkgPackageManager *pm);

private:
    Transaction *m_trans;

    pid_t m_child_id;
    bool m_startCounting;
    int m_progressBegin;
    int m_progressEnd;

    void updateInterface(int fd, int writeFd);
};

#endif
