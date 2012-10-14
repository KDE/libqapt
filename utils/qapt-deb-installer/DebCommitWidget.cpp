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

#include "DebCommitWidget.h"

// Qt includes
#include <QtGui/QLabel>
#include <QtGui/QProgressBar>
#include <QtGui/QTextEdit>
#include <QtGui/QVBoxLayout>

// KDE includes
#include <KMessageBox>

// LibQApt/DebconfKDE includes
#include <DebconfGui.h>
#include <LibQApt/Transaction>

// KDE includes
#include <KLocale>

DebCommitWidget::DebCommitWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);

    m_headerLabel = new QLabel(this);
    m_headerLabel->setText(i18nc("@info The widget's header label",
                                 "<title>Installing</title>"));

    m_terminal = new QTextEdit(this);
    m_terminal->setReadOnly(true);
    m_terminal->setFontFamily(QLatin1String("Monospace"));
    m_terminal->setWordWrapMode(QTextOption::NoWrap);
    m_terminal->setUndoRedoEnabled(false);
    QPalette p = m_terminal->palette();
    p.setColor(QPalette::Base, Qt::black);
    p.setColor(QPalette::Text, QColor(178, 178, 178));
    m_terminal->setPalette(p);
    m_terminal->setFrameShape(QFrame::NoFrame);

    m_debconfGui = new DebconfKde::DebconfGui("/tmp/qapt-sock", this);
    m_debconfGui->connect(m_debconfGui, SIGNAL(activated()), this, SLOT(showDebconf()));
    m_debconfGui->connect(m_debconfGui, SIGNAL(deactivated()), this, SLOT(hideDebconf()));
    m_debconfGui->hide();

    m_progressBar = new QProgressBar(this);
    m_progressBar->hide();

    layout->addWidget(m_headerLabel);
    layout->addWidget(m_terminal);
    layout->addWidget(m_debconfGui);
    layout->addWidget(m_progressBar);
}

void DebCommitWidget::setTransaction(QApt::Transaction *trans)
{
    m_trans = trans;

    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(statusChanged(QApt::TransactionStatus)));
    connect(m_trans, SIGNAL(errorOccurred(QApt::ErrorCode)),
            this, SLOT(transactionErrorOccurred(QApt::ErrorCode)));
}

void DebCommitWidget::statusChanged(QApt::TransactionStatus status)
{
}

void DebCommitWidget::errorOccurred(QApt::ErrorCode error)
{
    QString text;
    QString title;
    QString details;

    switch (error) {
        case QApt::InitError: {
            text = i18nc("@label",
                        "The package system could not be initialized, your "
                        "configuration may be broken.");
            title = i18nc("@title:window", "Initialization error");
            details = m_trans->errorDetails();
            KMessageBox::detailedError(this, text, details, title);
            break;
        }
        case QApt::WrongArchError:
            text = i18nc("@label",
                         "This package is incompatible with your computer.");
            title = i18nc("@title:window", "Incompatible Package");
            details =  m_trans->errorDetails();
            KMessageBox::detailedError(this, text, details, title);
            break;
        default:
            break;
    }
}

void DebCommitWidget::updateProgress(int progress)
{
    if (progress > 100) {
        m_progressBar->setMaximum(0);
        return;
    } else
        m_progressBar->setMaximum(100);

    m_progressBar->setValue(progress);
}

void DebCommitWidget::updateTerminal(const QString &message)
{
    m_terminal->insertPlainText(message);
}

void DebCommitWidget::showDebconf()
{
    m_terminal->hide();
    m_debconfGui->show();
}

void DebCommitWidget::hideDebconf()
{
    m_debconfGui->hide();
    m_terminal->show();
}

void DebCommitWidget::showProgress()
{
    m_terminal->hide();
    m_progressBar->show();
}

void DebCommitWidget::hideProgress()
{
    m_progressBar->hide();
    m_terminal->show();
}
