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
#include <QtGui/QStackedWidget>

#include <KApplication>
#include <KIcon>
#include <KLocale>
#include <KPushButton>
#include <KMessageBox>
#include <KDebug>

#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/version.h>

#include "../../src/backend.h"
#include "../../src/config.h"

#include "DebCommitWidget.h"
#include "DebViewer.h"

DebInstaller::DebInstaller(QWidget *parent, const QString &debFile)
    : KDialog(parent)
    , m_backend(new QApt::Backend)
    , m_debFile(debFile)
    , m_commitWidget(0)
    , m_canInstall(false)
{
    m_backend->init();
    connect(m_backend, SIGNAL(errorOccurred(QApt::ErrorCode, QVariantMap)),
            this, SLOT(errorOccurred(QApt::ErrorCode, QVariantMap)));

    initGUI();
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
    m_cancelButton = button(KDialog::Cancel);

    connect(m_applyButton, SIGNAL(clicked()), this, SLOT(installDebFile()));

    m_stack = new QStackedWidget(this);
    setMainWidget(m_stack);

    m_debViewer = new DebViewer(m_stack);
    m_stack->addWidget(m_debViewer);

    if (!m_debFile.isValid()) {
        QString text = i18nc("@label",
                             "Could not open <filename>%1</filename>. It does not appear to be a "
                             "valid Debian package file.", m_debFile.filePath());
        KMessageBox::error(this, text, QString());
        KApplication::instance()->quit();
        return;
    }

    setWindowTitle(i18nc("@title:window",
                         "Package Installer - %1", m_debFile.packageName()));
    m_debViewer->setDebFile(&m_debFile);
    checkDeb();

    m_applyButton->setEnabled(m_canInstall);
    m_debViewer->setStatusText(m_statusString);

    if (!m_versionInfo.isEmpty()){
        m_debViewer->setVersionTitle(m_versionTitle);
        m_debViewer->setVersionInfo(m_versionInfo);
    } else {
        m_debViewer->hideVersionInfo();
    }
}

void DebInstaller::workerEvent(QApt::WorkerEvent event)
{
    switch (event) {
    case QApt::DebInstallStarted:
        if (m_commitWidget) {
            m_stack->setCurrentWidget(m_commitWidget);
        }
        break;
    case QApt::DebInstallFinished:
        if (m_commitWidget) {
            m_commitWidget->updateTerminal(i18nc("@label Message that the install is done",
                                                "Done"));
            m_commitWidget->setHeaderText(i18nc("@info The widget's header label",
                                                "<title>Done</title>"));
        }
        setButtons(KDialog::Close);
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
            KApplication::instance()->quit();
            break;
        }
        case QApt::WrongArchError:
            text = i18nc("@label",
                         "This package is incompatible with your computer.");
            KMessageBox::error(this, text, QString());
            break;
        case QApt::AuthError:
            m_applyButton->setEnabled(true);
            m_cancelButton->setEnabled(true);
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
    m_applyButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
    initCommitWidget();
}

void DebInstaller::initCommitWidget()
{
    m_commitWidget = new DebCommitWidget(this);
    m_stack->addWidget(m_commitWidget);

    connect(m_backend, SIGNAL(debInstallMessage(const QString &)),
            m_commitWidget, SLOT(updateTerminal(const QString &)));
}

void DebInstaller::checkDeb()
{
    QString arch = m_backend->config()->readEntry(QLatin1String("APT::Architecture"),
                                                  QLatin1String(""));
    QString debArch = m_debFile.architecture();

    if (debArch != QLatin1String("all") && arch != debArch) {
        // Wrong arch
        m_statusString = i18nc("@info", "Error: Wrong architecture '%1'", debArch);
        m_statusString.prepend(QLatin1String("<font color=\"#ff0000\">"));
        m_statusString.append(QLatin1String("</font>"));
        m_canInstall = false;
        return;
    }

    compareDebWithCache();

    m_statusString = i18nc("@info", "All dependencies are satisfied.");
    m_canInstall = true;
}

void DebInstaller::compareDebWithCache()
{
    QApt::Package *pkg = m_backend->package(m_debFile.packageName());

    if (!pkg) {
        return;
    }

    QString version = m_debFile.version();

    int res = compareVersions(m_debFile.version().toStdString().c_str(),
                              pkg->availableVersion().toStdString().c_str());

    if (res == 0 && !pkg->isInstalled()) {
        m_versionTitle = i18nc("@info", "The same version is available in a software channel.");
        m_versionInfo = i18nc("@info", "It is recommended to install the software from the channel instead");
    } else if (res > 0) {
        m_versionTitle = i18nc("@info", "An older version is available in a software channel.");
        m_versionInfo = i18nc("@info", "It is recommended to install the version from the "
                                       "software channel, since it usually has more support.");
    } else if (res < 0) {
        m_versionTitle = i18nc("@info", "A newer version is available in a software channel.");
        m_versionInfo = i18nc("@info", "It is strongly advised to install the version from the "
                                       "software channel, since it usually has more support.");
    }
}

int DebInstaller::compareVersions(const char *A, const char *B)
{
    int LenA = strlen(A);
    int LenB = strlen(B);

    return _system->VS->DoCmpVersion(A, A+LenA,
                                     B, B+LenB);
}

#include "DebInstaller.moc"
