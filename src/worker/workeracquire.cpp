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

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QEventLoop>

#include <apt-pkg/error.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/acquire-worker.h>

#include "worker.h"

using namespace std;

WorkerAcquire::WorkerAcquire(QAptWorker *parent)
        : QObject(parent)
        , m_worker(parent)
        , m_canceled(false)
        , m_calculatingSpeed(true)
        , m_questionResponse(QVariantMap())
{
    MorePulses = true;
}

WorkerAcquire::~WorkerAcquire()
{
}

void WorkerAcquire::Start()
{
    // Cleanup from old fetches
    m_canceled = false;
    m_calculatingSpeed = true;

    pkgAcquireStatus::Start();
}

void WorkerAcquire::IMSHit(pkgAcquire::ItemDesc &item)
{
    QString message = QString::fromUtf8(item.Description.c_str());
    emit downloadMessage(QApt::HitFetch, message);
    updateStatus(item, /*percentage*/ -1, QApt::HitFetch);

    Update = true;
}

void WorkerAcquire::Fetch(pkgAcquire::ItemDesc &item)
{
    Update = true;
    if (item.Owner->Complete == true) {
        return;
    }

    QString message = QString::fromUtf8(item.Description.c_str());
    emit downloadMessage(QApt::DownloadFetch, message);
    updateStatus(item, /*percentage*/ -1, QApt::QueueFetch);
}

void WorkerAcquire::Done(pkgAcquire::ItemDesc &item)
{
   Update = true;

   updateStatus(item, 100, QApt::DownloadFetch);
};

void WorkerAcquire::Fail(pkgAcquire::ItemDesc &item)
{
    // Ignore certain kinds of transient failures (bad code)
    if (item.Owner->Status == pkgAcquire::Item::StatIdle) {
        return;
    }

    if (item.Owner->Status == pkgAcquire::Item::StatDone)
    {
        QString message = QString::fromUtf8(item.Description.c_str());
        emit downloadMessage(QApt::IgnoredFetch, message);
        updateStatus(item, /*percentage*/ -1, QApt::IgnoredFetch);
    } else {
        // an error was found (maybe 404, 403...)
        // the item that got the error and the error text
        QVariantMap args;
        args[QLatin1String("FailedItem")] = QString::fromUtf8(item.Description.c_str());
        args[QLatin1String("WarningText")] = QString::fromUtf8(item.Owner->ErrorText.c_str());
        emit fetchWarning(QApt::FetchFailedWarning, args);
    }

    Update = true;
}

void WorkerAcquire::Stop()
{
   pkgAcquireStatus::Stop();
}

bool WorkerAcquire::MediaChange(string Media, string Drive)
{
    QVariantMap args;
    args[QLatin1String("Media")] = QString::fromUtf8(Media.c_str());
    args[QLatin1String("Drive")] = QString::fromUtf8(Drive.c_str());

    QVariantMap result = askQuestion(QApt::MediaChange, args);

    bool mediaChanged = result[QLatin1String("MediaChanged")].toBool();

    return mediaChanged;
}

bool WorkerAcquire::Pulse(pkgAcquire *Owner)
{
    // FIXME: processEvents() is dangerous. Proper threading is needed
    QCoreApplication::processEvents();
    pkgAcquireStatus::Pulse(Owner);

    int packagePercentage = 0;
    for (pkgAcquire::Worker *iter = Owner->WorkersBegin(); iter != 0; iter = Owner->WorkerStep(iter)) {
        if (!iter->CurrentItem) {
            continue;
        }

        packagePercentage = qRound(double(iter->CurrentSize * 100.0)/double(iter->TotalSize));

        if (iter->TotalSize > 0) {
            updateStatus(*iter->CurrentItem, packagePercentage, QApt::DownloadFetch);
        } else {
            updateStatus(*iter->CurrentItem, 100, QApt::DownloadFetch);
        }
    }

    int percentage = qRound(double((CurrentBytes + CurrentItems) * 100.0)/double (TotalBytes + TotalItems));
    // work-around a stupid problem with libapt-pkg
    if(CurrentItems == TotalItems) {
        percentage = 100;
    }

    int speed;
    // m_calculatingSpeed is always set to true in the constructor since APT
    // will always have a period of time where it has to initially calulate
    // speed. Once speed > 0 for the first time, it'll be set to false, and
    // all subsequent zero values will be legitimate. APT should really do
    // this for us, but I guess stuff might depend on the old behavior...
    // The fail stops here.
    if (m_calculatingSpeed && CurrentCPS <= 0) {
        speed = -1;
    } else {
        m_calculatingSpeed = false;
        speed = CurrentCPS;
    }

   unsigned long long ETA = 0;
   if(CurrentCPS > 0)
        ETA = (TotalBytes - CurrentBytes) / CurrentCPS;

    // if the ETA is greater than two weeks, show unknown time
    if (ETA > 14*24*60*60) {
        ETA = 0;
    }

    emit downloadProgress(percentage, speed, ETA);

    Update = false;

    return !m_canceled;
}

void WorkerAcquire::requestCancel()
{
    m_canceled = true;
    emit fetchError(QApt::UserCancelError, QVariantMap());
}

QVariantMap WorkerAcquire::askQuestion(int questionCode, const QVariantMap &args)
{
    m_mediaBlock = new QEventLoop;
    connect(m_worker, SIGNAL(answerReady(const QVariantMap&)),
            this, SLOT(setAnswer(const QVariantMap&)));

    emit workerQuestion(questionCode, args);
    m_mediaBlock->exec(); // Process blocked, waiting for answerReady signal over dbus

    return m_questionResponse;
}

void WorkerAcquire::setAnswer(const QVariantMap &answer)
{
    disconnect(m_worker, SIGNAL(answerReady(const QVariantMap&)),
               this, SLOT(setAnswer(const QVariantMap&)));
    m_questionResponse = answer;
    m_mediaBlock->quit();
}

void WorkerAcquire::updateStatus(const pkgAcquire::ItemDesc &Itm, int percentage, int status)
{
    if (Itm.Owner->ID == 0) {
          QString name = QLatin1String(Itm.ShortDesc.c_str());
          QString URI = QLatin1String(Itm.Description.c_str());
          qint64 size = Itm.Owner->FileSize;

          emit packageDownloadProgress(name, percentage, URI, size, status);
    }
}
