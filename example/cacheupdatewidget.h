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

#ifndef CACHEUPDATEWIDGET_H
#define CACHEUPDATEWIDGET_H

#include <QItemDelegate>
#include <QTextDocument>
#include <QWidget>

#include <KVBox>

#include <LibQApt/Globals>

class QLabel;
class QListView;
class QProgressBar;
class QStandardItemModel;
class QPushButton;

namespace QApt {
    class Transaction;
    class DownloadProgress;
}

class CacheUpdateWidget : public KVBox
{
    Q_OBJECT
public:
    CacheUpdateWidget(QWidget *parent);

    void clear();
    void setTransaction(QApt::Transaction *trans);

private:
    QApt::Transaction *m_trans;
    QStringList m_downloads;
    int m_lastRealProgress;

    QLabel *m_headerLabel;
    QListView *m_downloadView;
    QStandardItemModel *m_downloadModel;
    QProgressBar *m_totalProgress;
    QLabel *m_downloadSpeedLabel;
    QLabel *m_ETALabel;
    QPushButton *m_cancelButton;

private slots:
    void onTransactionStatusChanged(QApt::TransactionStatus status);
    void progressChanged(int progress);
    void downloadProgressChanged(const QApt::DownloadProgress &progress);
    void updateDownloadSpeed(quint64 speed);
    void updateETA(quint64 ETA);
    void addItem(const QString &message);

signals:
    void cancelCacheUpdate();
};

#endif
