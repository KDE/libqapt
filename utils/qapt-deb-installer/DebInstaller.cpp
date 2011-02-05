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

#include "DebInstaller.h"

#include <QtCore/QStringBuilder>

#include <KApplication>
#include <KIcon>
#include <KLocale>
#include <KPushButton>
#include <KMessageBox>

#include <LibQApt/Backend>

#include "DebViewer.h"

DebInstaller::DebInstaller(QWidget *parent, const QString &debFile)
    : KDialog(parent)
    , m_backend(new QApt::Backend)
    , m_debFile(debFile)
{
    m_backend->init();
    connect(m_backend, SIGNAL(errorOccurred(QApt::ErrorCode, QVariantMap)),
            this, SLOT(errorOccurred(QApt::ErrorCode, QVariantMap)));

    initGUI();

    if (!m_debFile.isValid()) {
        QString text = i18nc("@label",
                             "Could not open <filename>%1</filename>. It does not appear to be a "
                             "valid Debian package file.", m_debFile.filePath());
        KMessageBox::error(this, text, QString());
        KApplication::exit();
    }

    DebViewer *debViewer = new DebViewer(this);
    setMainWidget(debViewer);
    debViewer->setDebFile(&m_debFile);
}

DebInstaller::~DebInstaller()
{
    delete m_backend;
}

void DebInstaller::initGUI()
{
    setButtons(KDialog::Cancel | KDialog::Apply);
    setButtonText(KDialog::Apply, i18nc("@label", "Install Package"));
    m_applyButton = button(KDialog::Apply);
    //m_applyButton->setEnabled(false);

    connect(m_applyButton, SIGNAL(clicked()), this, SLOT(installDebFile()));
}

void DebInstaller::workerEvent(QApt::WorkerEvent event)
{
    switch (event) {
    case QApt::DebInstallStarted:
        break;
    case QApt::DebInstallFinished:
        break;
    case QApt::InvalidEvent:
    default:
        break;
    }
}

void DebInstaller::errorOccurred(QApt::ErrorCode error, const QVariantMap &args)
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
            QString details = args["ErrorText"].toString();
            KMessageBox::detailedError(this, text, details, title);
            //FIXME: Quit
            break;
        }
        case QApt::WrongArchError:
            text = i18nc("@label",
                         "This package is incompatible with your computer.");
            KMessageBox::error(this, text, QString());
            break;
        case QApt::UnknownError:
        default:
            break;
    }
}

void DebInstaller::installDebFile()
{
    connect(m_backend, SIGNAL(workerEvent(QApt::WorkerEvent)),
            this, SLOT(workerEvent(QApt::WorkerEvent)));

    m_backend->installDebFile(m_debFile);
}

#include "DebInstaller.moc"
