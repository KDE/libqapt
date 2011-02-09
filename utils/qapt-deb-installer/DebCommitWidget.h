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

#ifndef DEBCOMMITWIDGET_H
#define DEBCOMMITWIDGET_H

#include <QtGui/QWidget>

class QLabel;
class QProgressBar;
class QTextEdit;

namespace DebconfKde
{
    class DebconfGui;
}

class DebCommitWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DebCommitWidget(QWidget *parent = 0);
    ~DebCommitWidget();

private:
    QLabel *m_headerLabel;
    QTextEdit *m_terminal;
    DebconfKde::DebconfGui *m_debconfGui;
    QProgressBar *m_progressBar;

public Q_SLOTS:
    void updateDownloadProgress(int progress, int speed, int ETA);
    void updateCommitProgress(const QString &message, int progress);
    void updateTerminal(const QString &message);
    void setHeaderText(const QString &text);
    void showProgress();
    void hideProgress();

private Q_SLOTS:
    void showDebconf();
    void hideDebconf();
};

#endif
