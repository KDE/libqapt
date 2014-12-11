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

#ifndef DEBVIEWER_H
#define DEBVIEWER_H

#include <QWidget>

#include <QApt/Globals>

class QLabel;
class QPushButton;
class QTextBrowser;

namespace QApt {
    class Backend;
    class DebFile;
}

class DebViewer : public QWidget
{
    Q_OBJECT
public:
    explicit DebViewer(QWidget *parent);
    ~DebViewer();

private:
    QApt::Backend *m_backend;
    QApt::DebFile *m_debFile;
    QApt::CacheState m_oldCacheState;

    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
    QLabel *m_statusLabel;
    QPushButton *m_detailsButton;
    QWidget *m_versionInfoWidget;
    QLabel *m_versionTitleLabel;
    QLabel *m_versionInfoLabel;
    QTextBrowser *m_descriptionWidget;
    QLabel *m_versionLabel;
    QLabel *m_sizeLabel;
    QLabel *m_maintainerLabel;
    QLabel *m_sectionLabel;
    QLabel *m_homepageLabel;
    QTextBrowser *m_fileWidget;

public Q_SLOTS:
    void setBackend(QApt::Backend *backend);
    void setDebFile(QApt::DebFile *debFile);
    void setStatusText(const QString &text);
    void showDetailsButton(bool show);
    void hideVersionInfo();
    void setVersionTitle(const QString &title);
    void setVersionInfo(const QString &info);

private Q_SLOTS:
    void detailsButtonClicked();
};

#endif
