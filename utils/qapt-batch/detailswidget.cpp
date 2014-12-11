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

#include "detailswidget.h"

// Qt includes
#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QDebug>

// KDE includes
#include <KFormat>
#include <KLocalizedString>
#include <QVBoxLayout>

// LibQApt includes
#include <QApt/Transaction>

DetailsWidget::DetailsWidget(QWidget *parent)
    : QWidget(parent)
    , m_trans(nullptr)
{
    QGridLayout *layout = new QGridLayout(this);

    QVBoxLayout *columnOne = new QVBoxLayout;
    QVBoxLayout *columnTwo = new QVBoxLayout;

    layout->addLayout(columnOne, 0, 0);
    layout->addLayout(columnTwo, 0, 1);
    setLayout(layout);

    QLabel *label1 = new QLabel(this);
    label1->setText(i18nc("@label Remaining time", "Remaining Time:"));
    label1->setAlignment(Qt::AlignRight);
    columnOne->addWidget(label1);

    QLabel *label2 = new QLabel(this);
    label2->setText(i18nc("@label Download Rate", "Speed:"));
    label2->setAlignment(Qt::AlignRight);
    columnOne->addWidget(label2);

    m_timeLabel = new QLabel(this);
    columnTwo->addWidget(m_timeLabel);
    m_speedLabel = new QLabel(this);
    columnTwo->addWidget(m_speedLabel);
}

void DetailsWidget::setTransaction(QApt::Transaction *trans)
{
    m_trans = trans;

    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(transactionStatusChanged(QApt::TransactionStatus)));
    connect(m_trans, SIGNAL(downloadETAChanged(quint64)),
            this, SLOT(updateTimeText(quint64)));
    connect(m_trans, SIGNAL(downloadSpeedChanged(quint64)),
            this, SLOT(updateSpeedText(quint64)));
}

void DetailsWidget::transactionStatusChanged(QApt::TransactionStatus status)
{
    // Limit visibility of details to when details exist
    switch (status) {
    case QApt::DownloadingStatus:
        show();
        break;
    case QApt::CommittingStatus:
    case QApt::FinishedStatus:
        hide();
        if (parentWidget()) {
             // Update size to prevent the dialog from looking silly.
            parentWidget()->adjustSize();
        }
        break;
    default:
        break;
    }
}

void DetailsWidget::updateTimeText(quint64 eta)
{
    QString timeRemaining;
    quint64 ETAMilliseconds = eta * 1000;

    // Greater than zero and less than 2 days
    if (ETAMilliseconds > 0 && ETAMilliseconds < 2*24*60*60) {
        timeRemaining = KFormat().formatDuration(ETAMilliseconds);
    } else {
        timeRemaining = i18nc("@info:progress Remaining time", "Unknown");
    }

    m_timeLabel->setText(timeRemaining);
}

void DetailsWidget::updateSpeedText(quint64 speed)
{
    QString downloadSpeed = i18nc("@info:progress Download rate",
                                  "%1/s", KFormat().formatByteSize(speed));
    m_speedLabel->setText(downloadSpeed);
}
