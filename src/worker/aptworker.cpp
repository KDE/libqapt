/***************************************************************************
 *   Copyright © 20120-2012 Jonathan Thomas <echidnaman@kubuntu.org>       *
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

#include "aptworker.h"

// Apt-pkg includes
#include <apt-pkg/configuration.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>

// Own includes
#include "cache.h"
#include "transaction.h"

AptWorker::AptWorker(QObject *parent)
    : QObject(parent)
    , m_cache(nullptr)
    , m_records(nullptr)
    , m_trans(nullptr)
    , m_ready(false)
{
}

AptWorker::~AptWorker()
{
    delete m_records;
}

void AptWorker::init()
{
    if (m_ready)
        return;

    pkgInitConfig(*_config);
    pkgInitSystem(*_config, _system);
    m_cache = new QApt::Cache(this);
    m_ready = true;
}

void AptWorker::runTransaction(Transaction *trans)
{
    // Check for running transactions or uninitialized worker
    if (m_trans || !m_ready)
        return;

    m_trans = trans;
    trans->setStatus(QApt::RunningStatus);
    waitForLock();

    // Process transactions requiring no cache
    switch (trans->role()) {
    case QApt::UpdateXapianRole:
        break;
    default: // The transaction must need a cache
        openCache();
        break;
    }

    // Process transactions requiring a cache
    switch (trans->role()) {
    // Transactions that can use a broken cache
    case QApt::UpdateCacheRole:
        //updateCache();
        break;
    // Transactions that need a consistent cache
    case QApt::UpgradeSystemRole:
        //upgradeSystem();
        break;
    // Other
    case QApt::EmptyRole:
    default:
        break;
    }

    // Cleanup
    trans->setProgress(100);
    //unlockCache();

    // Set transaction exit status
    // This will notify the transaction queue of the transaction's completion
    // as well as mark the transaction for deletion in 5 seconds
    if (trans->isCancelled())
        trans->setExitStatus(QApt::ExitCancelled);
    else if (trans->error() != QApt::Success)
        trans->setExitStatus(QApt::ExitFailed);
    else
        trans->setExitStatus(QApt::ExitSuccess);

    m_trans = nullptr;
}

void AptWorker::waitForLock()
{
}

void AptWorker::openCache()
{
    m_cache->open();

    delete m_records;
    m_records = new pkgRecords(*(m_cache->depCache()));
}
