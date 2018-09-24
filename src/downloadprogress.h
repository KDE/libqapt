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

#ifndef DOWNLOADPROGRESS_H
#define DOWNLOADPROGRESS_H

#include <QSharedDataPointer>
#include <QString>

#include "globals.h"

class QDBusArgument;

namespace QApt {

class DownloadProgressPrivate;

/**
 * The DownloadProgress class contains progress information for a single
 * file being downloaded through a Transaction.
 *
 * @author Jonathan Thomas
 * @since 2.0
 */
class Q_DECL_EXPORT DownloadProgress
{
public:
    DownloadProgress();

    /**
     * Constructs a new download progress from the given data.
     */
    DownloadProgress(const QString &uri, QApt::DownloadStatus status,
                     const QString &shortDesc, quint64 fileSize,
                     quint64 fetchedSize, const QString &statusMessage);

    /**
     * Constructs a copy of the @a other download progress.
     */
    DownloadProgress(const DownloadProgress &other);

    /**
     * Destroys the download progress.
     */
    ~DownloadProgress();

    /**
     * Assigns @a other to this download progress and returns a reference
     * to this download progress.
     */
    DownloadProgress &operator=(const DownloadProgress &rhs);

    /**
     * Returns the uniform resource identifier for the file being
     * downloaded. (Its remote path.)
     */
    QString uri() const;

    /**
     * Returns the current status for this download.
     */
    QApt::DownloadStatus status() const;

    /**
     * Returns a one-line short description of the file being downloaded.
     * (E.g. a package name or the name of a package list file that is being
     * downloaded.)
     */
    QString shortDescription() const;

    /**
     * Returns the full size of the file being downloaded.
     *
     * @sa partialSize()
     */
    quint64 fileSize() const;

    /**
     * Returns the size of the data that has already been downloaded for
     * the file.
     */
    quint64 fetchedSize() const;

    /**
     * Returns the current status message for the download.
     */
    QString statusMessage() const;

    /**
     * Returns the download progress of the URI, described in percentage.
     */
    int progress() const;

    /**
     * Convenience function to register DownloadProgress with the Qt
     * meta-type system for use with queued signals/slots or D-Bus.
     */
    static void registerMetaTypes();

    friend const QDBusArgument &operator>>(const QDBusArgument &argument,
                                           QApt::DownloadProgress &progress);
    friend QDBusArgument &operator<<(QDBusArgument &argument,
                                     const QApt::DownloadProgress &progress);

private:
    QSharedDataPointer<DownloadProgressPrivate> d;

    void setUri(const QString &uri);
    void setStatus(QApt::DownloadStatus status);
    void setShortDescription(const QString &shortDescription);
    void setFileSize(quint64 fileSize);
    void setFetchedSize(quint64 fetchedSize);
    void setStatusMessage(const QString &message);
};

}

Q_DECLARE_TYPEINFO(QApt::DownloadProgress, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QApt::DownloadProgress)

#endif // DOWNLOADPROGRESS_H
