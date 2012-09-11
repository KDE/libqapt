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

#include "commitwidget.h"

//Qt
#include <QLabel>
#include <QProgressBar>

//QApt
#include <LibQApt/Transaction>

CommitWidget::CommitWidget(QWidget *parent)
    : KVBox(parent)
    , m_trans(0)
{
    m_commitLabel = new QLabel(this);
    m_progressBar = new QProgressBar(this);
}

void CommitWidget::setTransaction(QApt::Transaction *trans)
{
    m_trans = trans;
    connect(m_trans, SIGNAL(statusDetailsChanged(QString)),
            m_commitLabel, SLOT(setText(QString)));
    connect(m_trans, SIGNAL(progressChanged(int)),
            m_progressBar, SLOT(setValue(int)));
    m_progressBar->setValue(trans->progress());
}

void CommitWidget::clear()
{
    m_commitLabel->setText(QString());
    m_progressBar->setValue(0);
    m_trans = 0;
}

#include "commitwidget.moc"
