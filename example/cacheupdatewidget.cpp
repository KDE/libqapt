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

#include <QLabel>
#include <QListView>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardItemModel>

#include <KIcon>
#include <KGlobal>
#include <KLocale>
#include <KDebug>

#include <LibQApt/Transaction>

CacheUpdateWidget::CacheUpdateWidget(QWidget *parent)
    : KVBox(parent)
    , m_trans(0)
    , m_lastRealProgress(0)
{
    m_headerLabel = new QLabel(this);

    m_downloadView = new QListView(this);

    m_downloadModel = new QStandardItemModel(this);
    m_downloadView->setModel(m_downloadModel);

    m_downloadSpeedLabel = new QLabel(this);
    m_ETALabel = new QLabel(this);
    m_totalProgress = new QProgressBar(this);

    m_cancelButton = new QPushButton(this);
    m_cancelButton->setText(i18n("Cancel"));
    m_cancelButton->setIcon(KIcon("dialog-cancel"));
}

void CacheUpdateWidget::clear()
{
    m_downloadModel->clear();
    m_downloads.clear();
    m_totalProgress->setValue(0);
}

void CacheUpdateWidget::setTransaction(QApt::Transaction *trans)
{
    m_trans = trans;
    clear();
    m_cancelButton->setEnabled(m_trans->isCancellable());
    connect(m_cancelButton, SIGNAL(pressed()),
            m_trans, SLOT(cancel()));

    // Listen for changes to the transaction
    connect(m_trans, SIGNAL(cancellableChanged(bool)),
            m_cancelButton, SLOT(setEnabled(bool)));
    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(onTransactionStatusChanged(QApt::TransactionStatus)));
    connect(m_trans, SIGNAL(progressChanged(int)),
            this, SLOT(progressChanged(int)));
    connect(m_trans, SIGNAL(downloadProgressChanged(QApt::DownloadProgress)),
            this, SLOT(downloadProgressChanged(QApt::DownloadProgress)));
    connect(m_trans, SIGNAL(downloadSpeedChanged(quint64)),
            this, SLOT(updateDownloadSpeed(quint64)));
    connect(m_trans, SIGNAL(downloadETAChanged(quint64)),
            this, SLOT(updateETA(quint64)));
}

void CacheUpdateWidget::addItem(const QString &message)
{
    QStandardItem *n = new QStandardItem();
    n->setText(message);
    m_downloadModel->appendRow(n);
    m_downloadView->scrollTo(m_downloadModel->indexFromItem(n));
}

void CacheUpdateWidget::updateDownloadSpeed(quint64 speed)
{
    QString downloadSpeed = i18n("Download rate: %1/s",
                                 KFormat().formatByteSize(speed));

    m_downloadSpeedLabel->setText(downloadSpeed);
}

void CacheUpdateWidget::updateETA(quint64 ETA)
{
    QString timeRemaining;
    int ETAMilliseconds = ETA * 1000;

    if (ETAMilliseconds <= 0 || ETAMilliseconds > 14*24*60*60*1000) {
        // If ETA is less than zero or bigger than 2 weeks
        timeRemaining = i18n("Unknown time remaining");
    } else {
        timeRemaining = i18n("%1 remaining", KGlobal::locale()->prettyFormatDuration(ETAMilliseconds));
    }
    m_ETALabel->setText(timeRemaining);
}

void CacheUpdateWidget::onTransactionStatusChanged(QApt::TransactionStatus status)
{
    QString headerText;

    qDebug() << "cache widget: transaction status changed" << status;

    switch (status) {
    case QApt::DownloadingStatus:
        if (m_trans->role == QApt::UpdateCacheRole)
            headerText = i18n("<b>Updating software sources</b>");
        else
            headerText = i18n("<b>Downloading Packages</b>");

        m_headerLabel->setText(headerText);
        break;
    default:
        break;
    }
}

void CacheUpdateWidget::progressChanged(int progress)
{
    if (progress > 100) {
        m_totalProgress->setMaximum(0);
    } else if (progress > m_lastRealProgress) {
        m_totalProgress->setMaximum(100);
        m_totalProgress->setValue(progress);
        m_lastRealProgress = progress;
    }
}

void CacheUpdateWidget::downloadProgressChanged(const QApt::DownloadProgress &progress)
{
    if (!m_downloads.contains(progress.uri())) {
        addItem(progress.uri());
        m_downloads.append(progress.uri());
    }
}
