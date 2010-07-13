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

// KDE includes
#include <KVBox>
#include <KLocale>

DetailsWidget::DetailsWidget(QWidget *parent)
    : QWidget(parent)
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

DetailsWidget::~DetailsWidget()
{
}

void DetailsWidget::setTimeText(const QString &text)
{
    m_timeLabel->setText(text);
}


void DetailsWidget::setSpeedText(const QString &text)
{
    m_speedLabel->setText(text);
}

#include "detailswidget.moc"
