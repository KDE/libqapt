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

#include "cacheupdatewidget.h"

#include <QtGui/QLabel>
#include <QtGui/QListView>
#include <QtGui/QProgressBar>
#include <QtGui/QPushButton>
#include <QStandardItemModel>

#include <KIcon>
#include <KGlobal>
#include <KLocale>

CacheUpdateWidget::CacheUpdateWidget(QWidget *parent)
    : KVBox(parent)
{
    m_headerLabel = new QLabel(this);

    m_downloadView = new QListView(this);

    m_downloadModel = new QStandardItemModel();
    m_downloadView->setModel(m_downloadModel);

    m_downloadLabel = new QLabel(this);
    m_totalProgress = new QProgressBar(this);

    m_cancelButton = new QPushButton(this);
    m_cancelButton->setText(i18n("Cancel"));
    m_cancelButton->setIcon(KIcon("dialog-cancel"));
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancelButtonPressed()));
}

CacheUpdateWidget::~CacheUpdateWidget()
{
}

void CacheUpdateWidget::clear()
{
    m_downloadModel->clear();
    m_totalProgress->setValue(0);
}

void CacheUpdateWidget::setHeaderText(const QString &text)
{
    m_headerLabel->setText(text);
}

void CacheUpdateWidget::addItem(const QString &message)
{
    QStandardItem *n = new QStandardItem();
    n->setText(message);
    m_downloadModel->appendRow(n);
    m_downloadView->scrollTo(m_downloadModel->indexFromItem(n));
}

void CacheUpdateWidget::setTotalProgress(int percentage, int speed, int ETA)
{
    m_totalProgress->setValue(percentage);

    QString downloadSpeed = KGlobal::locale()->formatByteSize(speed);

    QString timeRemaining;
    int ETAMilliseconds = ETA * 1000;

    if (ETAMilliseconds <= 0 || ETAMilliseconds > 14*24*60*60) {
        // If ETA is less than zero or bigger than 2 weeks
        timeRemaining = i18n("Unknown time");
    } else {
        timeRemaining = KGlobal::locale()->prettyFormatDuration(ETAMilliseconds);
    }
    m_downloadLabel->setText(i18n("Download Rate: %1/s - %2 remaining", downloadSpeed, timeRemaining));
}

void CacheUpdateWidget::cancelButtonPressed()
{
    emit(cancelCacheUpdate());
}

#include "cacheupdatewidget.moc"
