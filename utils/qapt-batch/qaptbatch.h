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

#ifndef QAPTBATCH_H
#define QAPTBATCH_H

// Qt includes
#include <QVariantMap>

// KDE includes
#include <KProgressDialog>

class QDBusServiceWatcher;

class OrgKubuntuQaptworkerInterface;
class DetailsWidget;

class QAptBatch : public KProgressDialog
{
    Q_OBJECT
public:
    explicit QAptBatch(QString mode, QStringList packages, int winId);
    virtual ~QAptBatch();
    virtual void reject();

private:
    OrgKubuntuQaptworkerInterface *m_worker;
    QDBusServiceWatcher *m_watcher;
    QString m_mode;
    QStringList m_packages;
    QList<QVariantMap> m_warningStack;
    QList<QVariantMap> m_errorStack;
    DetailsWidget *m_detailsWidget;
    bool m_done;

private Q_SLOTS:
    void commitChanges(int mode);
    void workerStarted();
    void errorOccurred(int code, const QVariantMap &args);
    void warningOccurred(int code, const QVariantMap &args);
    void questionOccurred(int question, const QVariantMap &args);
    void showQueuedWarnings();
    void showQueuedErrors();
    void raiseErrorMessage(const QString &text, const QString &title);
    void workerEvent(int event);
    void workerFinished(bool result);
    void serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

    void updateDownloadProgress(int percentage, int speed, int ETA);
    void updateCommitProgress(const QString& message, int percentage);
};

#endif
