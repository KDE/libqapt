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

#include "workeracquire.h"

// Qt includes
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QStringBuilder>

// Apt-pkg includes
#include <apt-pkg/error.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>

// Own includes
#include "aptworker.h"
#include "transaction.h"

#include <unistd.h>

using namespace std;

WorkerAcquire::WorkerAcquire(QObject *parent, int begin, int end)
        : QObject(parent)
        , m_calculatingSpeed(true)
        , m_progressBegin(begin)
        , m_progressEnd(end)
        , m_lastProgress(0)
{
    MorePulses = true;
}

void WorkerAcquire::setTransaction(Transaction *trans)
{
    m_trans = trans;
    if (!trans->proxy().isEmpty())
        setenv("http_proxy", m_trans->proxy().toLatin1(), 1);
}

void WorkerAcquire::Start()
{
    // Cleanup from old fetches
    m_calculatingSpeed = true;

    m_trans->setCancellable(true);
    m_trans->setStatus(QApt::DownloadingStatus);

    pkgAcquireStatus::Start();
}

void WorkerAcquire::IMSHit(pkgAcquire::ItemDesc &item)
{
    updateStatus(item);

    Update = true;
}

void WorkerAcquire::Fetch(pkgAcquire::ItemDesc &item)
{
    Update = true;
    if (item.Owner->Complete == true) {
        return;
    }

    updateStatus(item);
}

void WorkerAcquire::Done(pkgAcquire::ItemDesc &item)
{
   Update = true;

   updateStatus(item);
}

void WorkerAcquire::Fail(pkgAcquire::ItemDesc &item)
{
    // Ignore certain kinds of transient failures (bad code)
    if (item.Owner->Status == pkgAcquire::Item::StatIdle) {
        return;
    }

    if (item.Owner->Status == pkgAcquire::Item::StatDone) {
        updateStatus(item);
    } else {
        // an error was found (maybe 404, 403...)
        // the item that got the error and the error text
        QString failedItem = QString::fromStdString(item.URI);
        QString errorText = QString::fromStdString(item.Owner->ErrorText);

        m_trans->setErrorDetails(m_trans->errorDetails() % failedItem %
                                 '\n' % errorText % "\n\n");
    }

    Update = true;
}

void WorkerAcquire::Stop()
{
    m_trans->setProgress(m_progressEnd);
    m_trans->setCancellable(false);
    pkgAcquireStatus::Stop();
}

bool WorkerAcquire::MediaChange(string Media, string Drive)
{
    // Check if frontend can prompt for media
    if (!(m_trans->frontendCaps() & QApt::MediumPromptCap))
        return false;

    // Notify listeners to the transaction
    m_trans->setMediumRequired(QString::fromUtf8(Media.c_str()),
                               QString::fromUtf8(Drive.c_str()));
    m_trans->setStatus(QApt::WaitingMediumStatus);

    // Wait until the media is provided or the user cancels
    while (m_trans->isPaused())
        usleep(200000);

    m_trans->setStatus(QApt::DownloadingStatus);

    // As long as the user didn't cancel, we're ok to proceed
    return (!m_trans->isCancelled());
}

bool WorkerAcquire::Pulse(pkgAcquire *Owner)
{
    if (m_trans->isCancelled())
        return false;

    pkgAcquireStatus::Pulse(Owner);

    for (pkgAcquire::Worker *iter = Owner->WorkersBegin(); iter != 0; iter = Owner->WorkerStep(iter)) {
        if (!iter->CurrentItem) {
            continue;
        }

#if APT_PKG_ABI >= 590
        (*iter->CurrentItem).Owner->PartialSize = iter->CurrentItem->CurrentSize;
#else
        (*iter->CurrentItem).Owner->PartialSize = iter->CurrentSize;
#endif

        updateStatus(*iter->CurrentItem);
    }

    int percentage = qRound(double((CurrentBytes + CurrentItems) * 100.0)/double (TotalBytes + TotalItems));
    int progress = 0;
    // work-around a stupid problem with libapt-pkg
    if(CurrentItems == TotalItems) {
        percentage = 100;
    }

    // Calculate global progress, adjusted for given beginning and ending points
    progress = qRound(m_progressBegin + qreal(percentage / 100.0) * (m_progressEnd - m_progressBegin));

    if (m_lastProgress > progress)
        m_trans->setProgress(101);
    else {
        m_trans->setProgress(progress);
        m_lastProgress = progress;
    }

    quint64 ETA = 0;
    if(CurrentCPS > 0)
        ETA = (TotalBytes - CurrentBytes) / CurrentCPS;

    // if the ETA is greater than two days, show unknown time
    if (ETA > 2*24*60*60) {
        ETA = 0;
    }

    m_trans->setDownloadSpeed(CurrentCPS);
    m_trans->setETA(ETA);

    Update = false;
    return true;
}

void WorkerAcquire::updateStatus(const pkgAcquire::ItemDesc &Itm)
{
    QString URI = QString::fromStdString(Itm.Description);
    int status = (int)Itm.Owner->Status;
    QApt::DownloadStatus downloadStatus = QApt::IdleState;
    QString shortDesc = QString::fromStdString(Itm.ShortDesc);
    quint64 fileSize = Itm.Owner->FileSize;
    quint64 fetchedSize = Itm.Owner->PartialSize;
    QString errorMsg = QString::fromStdString(Itm.Owner->ErrorText);
    QString message;

    // Status mapping
    switch (status) {
    case pkgAcquire::Item::StatIdle:
        downloadStatus = QApt::IdleState;
        break;
    case pkgAcquire::Item::StatFetching:
        downloadStatus = QApt::FetchingState;
        break;
    case pkgAcquire::Item::StatDone:
        downloadStatus = QApt::DoneState;
        fetchedSize = fileSize;
        break;
    case pkgAcquire::Item::StatError:
        downloadStatus = QApt::ErrorState;
        break;
    case pkgAcquire::Item::StatAuthError:
        downloadStatus = QApt::AuthErrorState;
        break;
    case pkgAcquire::Item::StatTransientNetworkError:
        downloadStatus = QApt::NetworkErrorState;
        break;
    default:
        break;
    }

    if (downloadStatus == QApt::DoneState && errorMsg.size())
        message = errorMsg;
    else if (!Itm.Owner->ActiveSubprocess.empty())
        message = QString::fromStdString(Itm.Owner->ActiveSubprocess);

    QApt::DownloadProgress dp(URI, downloadStatus, shortDesc,
                              fileSize, fetchedSize, message);

    m_trans->setDownloadProgress(dp);
}
