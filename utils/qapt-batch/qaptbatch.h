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

#ifndef QAPTBATCH_H
#define QAPTBATCH_H

// KDE includes
#include <KProgressDialog>

// LibQApt includes
#include "../../src/globals.h"

namespace QApt {
    class Backend;
    class Transaction;
}

class QAptBatch : public KProgressDialog
{
    Q_OBJECT
public:
    explicit QAptBatch(QString mode, QStringList packages, int winId);

    void reject();

private:
    QApt::Backend *m_backend;
    QApt::Transaction *m_trans;

    int m_winId;
    int m_lastRealProgress;
    QString m_mode;
    QStringList m_packages;
    bool m_done;

    void setTransaction(QApt::Transaction *trans);

private Q_SLOTS:
    void initError();
    void commitChanges(int mode);
    void errorOccurred(QApt::ErrorCode code);
    void provideMedium(const QString &label, const QString &mountPoint);
    void untrustedPrompt(const QStringList &untrustedPackages);
    void raiseErrorMessage(const QString &text, const QString &title);
    void transactionStatusChanged(QApt::TransactionStatus status);
    void cancellableChanged(bool cancellable);

    void updateProgress(int progress);
    void updateCommitMessage(const QString& message);
};

#endif
