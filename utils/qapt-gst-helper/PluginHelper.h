/***************************************************************************
 *   Copyright © 2011 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#ifndef PLUGINHELPER_H
#define PLUGINHELPER_H

#include <QtCore/QStringList>

#include <KProgressDialog>

#include <../../src/globals.h>

class QThread;

namespace QApt {
    class Backend;
}

class PluginFinder;
class PluginInfo;

#define ERR_NO_PLUGINS 1
#define ERR_RANDOM_ERR 2
#define ERR_PARTIAL_SUCCESS 3
#define ERR_CANCEL 4

class PluginHelper : public KProgressDialog
{
    Q_OBJECT
public:
    explicit PluginHelper(QWidget *parent, const QStringList &details, int winId);
    ~PluginHelper();

    void run();

private:
    QApt::Backend *m_backend;
    int m_winId;
    bool m_partialFound;
    bool m_done;

    QStringList m_details;
    QList<PluginInfo *> m_searchList;
    QList<QApt::Package *> m_foundCodecs;
    QList<QVariantMap> m_warningStack;
    QList<QVariantMap> m_errorStack;

    QThread *m_finderThread;
    PluginFinder *m_finder;

private Q_SLOTS:
    void canSearch();
    void offerInstallPackages();
    void errorOccurred(QApt::ErrorCode code, const QVariantMap &args);
    void warningOccurred(QApt::WarningCode code, const QVariantMap &args);
    void questionOccurred(QApt::WorkerQuestion question, const QVariantMap &args);
    void showQueuedWarnings();
    void showQueuedErrors();
    void raiseErrorMessage(const QString &text, const QString &title);
    void workerEvent(QApt::WorkerEvent event);
    void foundCodec(QApt::Package *);
    void notFound();
    void notFoundError();
    void incrementProgress();
    void install();

    void updateDownloadProgress(int percentage, int speed, int ETA);
    void updateCommitProgress(const QString& message, int percentage);

    virtual void reject();
};

#endif
