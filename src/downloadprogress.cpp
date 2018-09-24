/***************************************************************************
 *   Copyright © 2012 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#include "downloadprogress.h"

// Qt includes
#include <QSharedData>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QDebug>

namespace QApt {

class DownloadProgressPrivate : public QSharedData
{
public:
    DownloadProgressPrivate()
        : QSharedData()
        , status(QApt::IdleState)
        , fileSize(0)
        , fetchedSize(0)
    {
    }

    DownloadProgressPrivate(const QString &dUri, QApt::DownloadStatus dStatus,
                            const QString &sDesc, quint64 fSize, quint64 pSize,
                            const QString &sMessage)
        : QSharedData()
        , uri(dUri)
        , status(dStatus)
        , shortDesc(sDesc)
        , fileSize(fSize)
        , fetchedSize(pSize)
        , statusMessage(sMessage)
    {
    }

    DownloadProgressPrivate(const DownloadProgressPrivate &other)
        : QSharedData(other)
    {
        uri = other.uri;
        status = other.status;
        shortDesc = other.shortDesc;
        fileSize = other.fileSize;
        fetchedSize = other.fetchedSize;
        statusMessage = other.statusMessage;
    }

    ~DownloadProgressPrivate() {}

    QString uri;
    QApt::DownloadStatus status;
    QString shortDesc;
    quint64 fileSize;
    quint64 fetchedSize;
    QString statusMessage;
};

DownloadProgress::DownloadProgress()
    : d(new DownloadProgressPrivate)
{
}

DownloadProgress::DownloadProgress(const QString &uri, QApt::DownloadStatus status,
                                   const QString &shortDesc, quint64 fileSize,
                                   quint64 partialSize, const QString &statusMessage)
    : d(new DownloadProgressPrivate(uri, status, shortDesc, fileSize, partialSize, statusMessage))
{
}

DownloadProgress::DownloadProgress(const DownloadProgress &other)
    : d(other.d)
{
}

DownloadProgress::~DownloadProgress()
{
}

DownloadProgress &DownloadProgress::operator=(const DownloadProgress &rhs)
{
    // Protect against self-assignment
    if (this == &rhs) {
        return *this;
    }
    d = rhs.d;
    return *this;
}

QString DownloadProgress::uri() const
{
    return d->uri;
}

void DownloadProgress::setUri(const QString &uri)
{
    d->uri = uri;
}

QApt::DownloadStatus DownloadProgress::status() const
{
    return d->status;
}

void DownloadProgress::setStatus(QApt::DownloadStatus status)
{
    d->status = status;
}

QString DownloadProgress::shortDescription() const
{
    return d->shortDesc;
}

void DownloadProgress::setShortDescription(const QString &shortDescription)
{
    d->shortDesc = shortDescription;
}

quint64 DownloadProgress::fileSize() const
{
    return d->fileSize;
}

void DownloadProgress::setFileSize(quint64 fileSize)
{
    d->fileSize = fileSize;
}

quint64 DownloadProgress::fetchedSize() const
{
    return d->fetchedSize;
}

void DownloadProgress::setFetchedSize(quint64 partialSize)
{
    d->fetchedSize = partialSize;
}

QString DownloadProgress::statusMessage() const
{
    return d->statusMessage;
}

void DownloadProgress::setStatusMessage(const QString &message)
{
    d->statusMessage = message;
}

int DownloadProgress::progress() const
{
    return (d->fileSize) ? qRound(double(d->fetchedSize * 100.0)/double(d->fileSize)) : 100;
}

void DownloadProgress::registerMetaTypes()
{
    qRegisterMetaType<QApt::DownloadProgress>("QApt::DownloadProgress");
    qDBusRegisterMetaType<QApt::DownloadProgress>();
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                DownloadProgress &progress)
{
    argument.beginStructure();
    QString uri;
    argument >> uri;
    progress.setUri(uri);

    int status;
    argument >> status;
    progress.setStatus((QApt::DownloadStatus)status);

    QString shortDesc;
    argument >> shortDesc;
    progress.setShortDescription(shortDesc);

    quint64 fileSize;
    argument >> fileSize;
    progress.setFileSize(fileSize);

    quint64 fetchedSize;
    argument >> fetchedSize;
    progress.setFetchedSize(fetchedSize);

    QString statusMessage;
    argument >> statusMessage;
    progress.setStatusMessage(statusMessage);

    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const DownloadProgress &progress)
{
    argument.beginStructure();
    argument << progress.uri() << (int)progress.status()
             << progress.shortDescription() << progress.fileSize()
             << progress.fetchedSize() << progress.statusMessage();
    argument.endStructure();

    return argument;
}

}
