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

#include "qaptbatch.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <KLocalizedString>
#include <KMessageBox>
#include <KProtocolManager>
#include <KStandardGuiItem>
#include <KWindowSystem>

#include <QApt/Backend>
#include <QApt/Transaction>

#include "detailswidget.h"

QAptBatch::QAptBatch(QString mode, QStringList packages, int winId)
    : QDialog()
    , m_backend(new QApt::Backend(this))
    , m_winId(winId)
    , m_lastRealProgress(0)
    , m_mode(mode)
    , m_packages(packages)
    , m_done(false)
    , m_label(new QLabel(this))
    , m_progressBar(new QProgressBar(this))
    , m_detailsWidget(new DetailsWidget(this))
    , m_buttonBox(new QDialogButtonBox(this))
{
    if (!m_backend->init())
        initError();

    // Set frontend capabilities
    QApt::FrontendCaps caps = (QApt::FrontendCaps)(QApt::DebconfCap | QApt::MediumPromptCap |
                               QApt::UntrustedPromptCap);
    m_backend->setFrontendCaps(caps);

    // Set this in case we auto-show before auth
    m_label->setText(i18nc("@label", "Waiting for authorization"));
    m_progressBar->setMaximum(0); // Set progress bar to indeterminate/busy

    if (m_mode == "install") {
        commitChanges(QApt::Package::ToInstall, packages);
    } else if (m_mode == "uninstall") {
        commitChanges(QApt::Package::ToRemove, packages);
    } else if (m_mode == "update") {
        m_trans = m_backend->updateCache();
    }
    m_detailsWidget->setTransaction(m_trans);
    setTransaction(m_trans);

    if (winId)
        KWindowSystem::setMainWindow(this, winId);

    // Create buttons.
    m_buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Close);

    setVisibleButtons(QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QAptBatch::reject);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(m_label);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_detailsWidget);
    layout->addWidget(m_buttonBox);
    setLayout(layout);
}

void QAptBatch::initError()
{
    QString details = m_backend->initErrorMessage();

    QString text = i18nc("@label",
                         "The package system could not be initialized, your "
                         "configuration may be broken.");
    QString title = i18nc("@title:window", "Initialization error");
    KMessageBox::detailedError(this, text, details, title);
    exit(-1);
}

void QAptBatch::reject()
{
    if (m_done)
        accept();
    else
        QDialog::reject();
}

void QAptBatch::commitChanges(int mode, const QStringList &packageStrs)
{
    QApt::PackageList packages;

    QApt::Package *pkg;
    for (const QString &packageStr : packageStrs) {
        pkg = m_backend->package(packageStr);

        if (pkg)
            packages.append(pkg);
        else {
            QString text = i18nc("@label",
                                 "The package \"%1\" has not been found among your software sources. "
                                 "Therefore, it cannot be installed. ",
                                 packageStr);
            QString title = i18nc("@title:window", "Package Not Found");
            KMessageBox::error(this, text, title);
            close();
        }
    }

    m_trans = (mode == QApt::Package::ToInstall) ?
               m_backend->installPackages(packages) :
               m_backend->removePackages(packages);
}

void QAptBatch::setTransaction(QApt::Transaction *trans)
{
    m_trans = trans;

    m_trans->setLocale(setlocale(LC_ALL, 0));

    // Provide proxy/locale to the transaction
    if (KProtocolManager::proxyType() == KProtocolManager::ManualProxy) {
        m_trans->setProxy(KProtocolManager::proxyFor("http"));
    }

    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(transactionStatusChanged(QApt::TransactionStatus)));
    connect(m_trans, SIGNAL(errorOccurred(QApt::ErrorCode)),
            this, SLOT(errorOccurred(QApt::ErrorCode)));
    connect(m_trans, SIGNAL(cancellableChanged(bool)),
            this, SLOT(cancellableChanged(bool)));
    connect(m_trans, SIGNAL(mediumRequired(QString,QString)),
            this, SLOT(provideMedium(QString,QString)));
    connect(m_trans, SIGNAL(promptUntrusted(QStringList)),
            this, SLOT(untrustedPrompt(QStringList)));
    connect(m_trans, SIGNAL(progressChanged(int)),
            this, SLOT(updateProgress(int)));
    connect(m_trans, SIGNAL(statusDetailsChanged(QString)),
            this, SLOT(updateCommitMessage(QString)));

    connect(this, SIGNAL(rejected()), m_trans, SLOT(cancel()));

    m_trans->run();
}

void QAptBatch::setVisibleButtons(QDialogButtonBox::StandardButtons buttons)
{
    m_buttonBox->button(QDialogButtonBox::Cancel)->setVisible(buttons & QDialogButtonBox::Cancel);
    m_buttonBox->button(QDialogButtonBox::Close)->setVisible(buttons & QDialogButtonBox::Close);
}

void QAptBatch::errorOccurred(QApt::ErrorCode code)
{
    QString text;
    QString title;

    switch(code) {
        case QApt::InitError: {
            text = i18nc("@label",
                         "The package system could not be initialized, your "
                         "configuration may be broken.");
            title = i18nc("@title:window", "Initialization error");
            KMessageBox::detailedError(this, text, m_trans->errorDetails(), title);
            QApplication::quit();
            break;
        }
        case QApt::LockError:
            text = i18nc("@label",
                         "Another application seems to be using the package "
                         "system at this time. You must close all other package "
                         "managers before you will be able to install or remove "
                         "any packages.");
            title = i18nc("@title:window", "Unable to obtain package system lock");
            raiseErrorMessage(text, title);
            break;
        case QApt::DiskSpaceError:
            text = i18nc("@label",
                         "You do not have enough disk space in the directory "
                         "at %1 to continue with this operation.", m_trans->errorDetails());
            title = i18nc("@title:window", "Low disk space");
            raiseErrorMessage(text, title);
            break;
        case QApt::FetchError:
            text = i18nc("@label",
                         "Could not download packages");
            title = i18nc("@title:window", "Download failed");
            raiseErrorMessage(text, title);
            break;
        case QApt::CommitError:
            text = i18nc("@label", "An error occurred while applying changes:");
            title = i18nc("@title:window", "Commit error");
            KMessageBox::detailedError(this, text, m_trans->errorDetails(), title);
            break;
        case QApt::AuthError:
            text = i18nc("@label",
                         "This operation cannot continue since proper "
                         "authorization was not provided");
            title = i18nc("@title:window", "Authentication error");
            raiseErrorMessage(text, title);
            break;
        case QApt::WorkerDisappeared:
            text = i18nc("@label", "It appears that the QApt worker has either crashed "
                         "or disappeared. Please report a bug to the QApt maintainers");
            title = i18nc("@title:window", "Unexpected Error");
            KMessageBox::error(this, text, title);
            close();
            break;
        case QApt::UntrustedError: {
            QStringList untrustedItems = m_trans->untrustedPackages();
            if (untrustedItems.size() == 1) {
                text = i18ncp("@label",
                             "The following package has not been verified by its author. "
                             "Downloading untrusted packages has been disallowed "
                             "by your current configuration.",
                             "The following packages have not been verified by "
                             "their authors. "
                             "Downloading untrusted packages has "
                             "been disallowed by your current configuration.",
                             untrustedItems.size());
            }
            title = i18nc("@title:window", "Untrusted Packages");
            KMessageBox::errorList(this, text, untrustedItems, title);
            close();
            break;
        }
        case QApt::NotFoundError: {
            text = i18nc("@label",
                        "The package \"%1\" has not been found among your software sources. "
                        "Therefore, it cannot be installed. ",
                        m_trans->errorDetails());
            title = i18nc("@title:window", "Package Not Found");
            KMessageBox::error(this, text, title);
            close();
            break;
        }
        case QApt::UnknownError:
        default:
            break;
    }
}

void QAptBatch::provideMedium(const QString &label, const QString &mountPoint)
{
    QString title = i18nc("@title:window", "Media Change Required");
    QString text = xi18nc("@label", "Please insert %1 into <filename>%2</filename>",
                          label, mountPoint);
    KMessageBox::informationWId(m_winId, text, title);
    m_trans->provideMedium(mountPoint);
}

void QAptBatch::untrustedPrompt(const QStringList &untrustedPackages)
{
    QString title = i18nc("@title:window", "Warning - Unverified Software");
    QString text = xi18ncp("@label",
                           "The following piece of software cannot be verified. "
                           "<warning>Installing unverified software represents a "
                           "security risk, as the presence of unverifiable software "
                           "can be a sign of tampering.</warning> Do you wish to continue?",
                           "The following pieces of software cannot be authenticated. "
                           "<warning>Installing unverified software represents a "
                           "security risk, as the presence of unverifiable software "
                           "can be a sign of tampering.</warning> Do you wish to continue?",
                           untrustedPackages.size());
    int result = KMessageBox::Cancel;

    result = KMessageBox::warningContinueCancelListWId(m_winId, text,
                                                       untrustedPackages, title);

    bool installUntrusted = (result == KMessageBox::Continue);
    m_trans->replyUntrustedPrompt(installUntrusted);

    if (!installUntrusted) {
        close();
    }
}

void QAptBatch::raiseErrorMessage(const QString &text, const QString &title)
{
    KMessageBox::error(0, text, title);
    close();
}

void QAptBatch::transactionStatusChanged(QApt::TransactionStatus status)
{
    switch (status) {
    case QApt::SetupStatus:
    case QApt::WaitingStatus:
        m_progressBar->setMaximum(0);
        m_label->setText(i18nc("@label Progress bar label when waiting to start",
                           "Waiting to start."));
        break;
    case QApt::AuthenticationStatus:
        m_progressBar->setMaximum(0);
        m_label->setText(i18nc("@label Status label when waiting for a password",
                           "Waiting for authentication."));
        break;
    case QApt::WaitingMediumStatus:
        m_label->setText(i18nc("@label Status label when waiting for a CD-ROM",
                           "Waiting for required media."));
        break;
    case QApt::WaitingLockStatus:
        m_progressBar->setMaximum(0);
        m_label->setText(i18nc("@label Status label",
                           "Waiting for other package managers to quit."));
        break;
    case QApt::RunningStatus:
        // We're ready for "real" progress now
        m_progressBar->setMaximum(100);
        break;
    case QApt::LoadingCacheStatus:
        m_label->setText(i18nc("@label Status label",
                           "Loading package cache."));
        break;
    case QApt::DownloadingStatus:
        if (m_trans->role() == QApt::UpdateCacheRole) {
            setWindowTitle(i18nc("@title:window", "Refreshing Package Information"));
            m_label->setText(i18nc("@info:status", "Checking for new, removed or upgradeable packages"));
        } else {
            setWindowTitle(i18nc("@title:window", "Downloading"));
            m_label->setText(i18ncp("@info:status",
                                "Downloading package file",
                                "Downloading package files",
                                m_packages.count()));
        }

        setVisibleButtons(QDialogButtonBox::Cancel);
        break;
    case QApt::CommittingStatus:
        setWindowTitle(i18nc("@title:window", "Installing Packages"));
        setVisibleButtons(QDialogButtonBox::Cancel);
        break;
    case QApt::FinishedStatus: {
        if (m_mode == "install") {
            setWindowTitle(i18nc("@title:window", "Installation Complete"));

            if (m_trans->error() != QApt::Success) {
                m_label->setText(i18nc("@label",
                                   "Package installation failed."));
            } else {
                m_label->setText(i18ncp("@label",
                                    "Package successfully installed.",
                                    "Packages successfully installed.", m_packages.size()));
            }
        } else if (m_mode == "uninstall") {
            setWindowTitle(i18nc("@title:window", "Removal Complete"));

            if (m_trans->error() != QApt::Success) {
                m_label->setText(i18nc("@label",
                                   "Package removal failed."));
                qDebug() << m_trans->error();
            } else {
                m_label->setText(i18ncp("@label",
                                    "Package successfully uninstalled.",
                                    "Packages successfully uninstalled.", m_packages.size()));
            }
        } else if (m_mode == "update") {
            setWindowTitle(i18nc("@title:window", "Refresh Complete"));

            if (m_trans->error() != QApt::Success) {
                m_label->setText(i18nc("@info:status", "Refresh failed."));
            } else {
                m_label->setText(i18nc("@label", "Package information successfully refreshed."));
            }
        }

        m_progressBar->setValue(100);
        m_done = true;

        m_trans->deleteLater();
        m_trans = 0;

        setVisibleButtons(QDialogButtonBox::Close);
        m_buttonBox->button(QDialogButtonBox::Close)->setFocus();
        break;
    }
    case QApt::WaitingConfigFilePromptStatus:
    default:
        break;
    }
}

void QAptBatch::cancellableChanged(bool cancellable)
{
    m_buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(cancellable);
}

void QAptBatch::updateProgress(int progress)
{
    if (progress == 100) {
        --progress;
    }

    if (progress > 100) {
        m_progressBar->setMaximum(0);
    } else if (progress > m_lastRealProgress) {
        m_progressBar->setMaximum(100);
        m_progressBar->setValue(progress);
        m_lastRealProgress = progress;
    }
}

void QAptBatch::updateCommitMessage(const QString& message)
{
    m_label->setText(message);
}
