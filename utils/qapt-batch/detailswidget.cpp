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
#include <QtGui/QGridLayout>
#include <QtGui/QLabel>
#include <QDebug>

// KDE includes
#include <KGlobal>
#include <KLocale>
#include <KVBox>

// LibQApt includes
#include "../../src/transaction.h"

DetailsWidget::DetailsWidget(QWidget *parent)
    : QWidget(parent)
    , m_trans(nullptr)
{
    QGridLayout *layout = new QGridLayout(this);

    KVBox *columnOne = new KVBox(this);
    KVBox *columnTwo = new KVBox(this);

    layout->addWidget(columnOne);
    layout->addWidget(columnTwo, 0, 1);
    setLayout(layout);

    QLabel *label1 = new QLabel(columnOne);
    label1->setText(i18nc("@label Remaining time", "Remaining Time:"));
    label1->setAlignment(Qt::AlignRight);

    QLabel *label2 = new QLabel(columnOne);
    label2->setText(i18nc("@label Download Rate", "Speed:"));
    label2->setAlignment(Qt::AlignRight);

    m_timeLabel = new QLabel(columnTwo);
    m_speedLabel = new QLabel(columnTwo);
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
        timeRemaining = KGlobal::locale()->prettyFormatDuration(ETAMilliseconds);
    } else {
        timeRemaining = i18nc("@info:progress Remaining time", "Unknown");
    }

    m_timeLabel->setText(timeRemaining);
}

void DetailsWidget::updateSpeedText(quint64 speed)
{
    QString downloadSpeed = i18nc("@info:progress Download rate",
                                  "%1/s", KGlobal::locale()->formatByteSize(speed));
    m_speedLabel->setText(downloadSpeed);
}
