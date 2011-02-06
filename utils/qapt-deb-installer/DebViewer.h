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

#include <QtGui/QWidget>

class QLabel;

class KTextBrowser;

namespace QApt {
    class DebFile;
}

class DebViewer : public QWidget
{
    Q_OBJECT
public:
    explicit DebViewer(QWidget *parent);
    ~DebViewer();

private:
    QApt::DebFile *m_debFile;

    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
    QLabel *m_statusLabel;
    QWidget *m_versionInfoWidget;
    QLabel *m_versionTitleLabel;
    QLabel *m_versionInfoLabel;
    KTextBrowser *m_descriptionWidget;
    QLabel *m_versionLabel;
    QLabel *m_sizeLabel;
    QLabel *m_maintainerLabel;
    QLabel *m_sectionLabel;
    QLabel *m_homepageLabel;
    KTextBrowser *m_fileWidget;

public Q_SLOTS:
    void setDebFile(QApt::DebFile *debFile);
    void setStatusText(const QString &text);
    void hideVersionInfo();
    void setVersionTitle(const QString &title);
    void setVersionInfo(const QString &info);
};

#endif
