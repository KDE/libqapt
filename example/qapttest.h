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

#ifndef QAPTTEST_H
#define QAPTTEST_H


#include <KMainWindow>

#include <../src/backend.h>

class QLabel;
class QStackedWidget;

class KToggleAction;
class KLineEdit;

class CacheUpdateWidget;

class qapttest : public KMainWindow
{
    Q_OBJECT
public:
    qapttest();

    virtual ~qapttest();

private Q_SLOTS:
    void updateLabels();
    void updateCache();
    void workerEvent(int code);
    void updateDownloadProgress(int percentage, int speed, int ETA);
    void updateDownloadMessage(int flag, const QString &name);
    void updateCommitProgress(const QString& message, int percentage);
    void updateStatusBar();

private:
    QApt::Backend *m_backend;
    QApt::Package *m_package;
    QApt::Group *m_group;

    QStackedWidget *m_stack;
    QWidget *m_mainWidget;
    CacheUpdateWidget *m_cacheUpdateWidget;
    KLineEdit *m_lineEdit;
    QLabel *m_nameLabel;
    QLabel *m_sectionLabel;
    QLabel *m_originLabel;
    QLabel *m_installedSizeLabel;
    QLabel *m_maintainerLabel;
    QLabel *m_sourceLabel;
    QLabel *m_versionLabel;
    QLabel *m_packageSizeLabel;
    QLabel *m_shortDescriptionLabel;
    QLabel *m_longDescriptionLabel;

    QLabel *m_installedCountLabel;
    QLabel *m_packageCountLabel;
};

#endif
