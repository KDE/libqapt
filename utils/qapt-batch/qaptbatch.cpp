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

// KDE includes
#include <KApplication>
#include <KDebug>
#include <KIcon>
#include <KLocale>
#include <KMessageBox>
#include <KProtocolManager>
#include <KWindowSystem>

#include "../../src/backend.h"
#include "../../src/transaction.h"
#include "detailswidget.h"

QAptBatch::QAptBatch(QString mode, QStringList packages, int winId)
    : KProgressDialog()
    , m_backend(new QApt::Backend(this))
    , m_winId(winId)
    , m_lastRealProgress(0)
    , m_mode(mode)
    , m_packages(packages)
    , m_done(false)
{
    m_backend->init();

    // Set this in case we auto-show before auth
    setLabelText(i18nc("@label", "Waiting for authorization"));
    progressBar()->setMaximum(0); // Set progress bar to indeterminate/busy

    DetailsWidget *detailsWidget = new DetailsWidget(this);
    setDetailsWidget(detailsWidget);

    if (m_mode == "install") {
        commitChanges(QApt::Package::ToInstall);
    } else if (m_mode == "uninstall") {
        commitChanges(QApt::Package::ToRemove);
    } else if (m_mode == "update") {
        m_trans = m_backend->updateCache();
    }

    detailsWidget->setTransaction(m_trans);
    setTransaction(m_trans);

    if (winId)
        KWindowSystem::setMainWindow(this, winId);

    setAutoClose(false);
}

void QAptBatch::reject()
{
    if (m_done)
        accept();
    else
        KProgressDialog::reject();
}

void QAptBatch::commitChanges(int mode)
{
    QStringList packageStrs;
    QApt::PackageList packages;

    QApt::Package *pkg;
    for (const QString &packageStr : packageStrs) {
        pkg = m_backend->package(packageStr);

        if (pkg)
            packages.append(pkg);
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
            this, SLOT(errorOccurred(int,QVariantMap)));
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

    connect(this, SIGNAL(cancelClicked()), m_trans, SLOT(cancel()));

    m_trans->run();
}

void QAptBatch::errorOccurred(QApt::ErrorCode code)
{
    QString text;
    QString title;
    QString drive;

    switch(code) {
        case QApt::InitError: {
            text = i18nc("@label",
                         "The package system could not be initialized, your "
                         "configuration may be broken.");
            title = i18nc("@title:window", "Initialization error");
            // FIXME: details = transaction error details
            QString details;
            KMessageBox::detailedError(this, text, details, title);
            KApplication::instance()->quit();
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
    QString text = i18nc("@label", "Please insert %1 into <filename>%2</filename>",
                         label, mountPoint);

    KMessageBox::informationWId(m_winId, text, title);
    m_trans->provideMedium(mountPoint);
}

void QAptBatch::untrustedPrompt(const QStringList &untrustedPackages)
{
    QString title = i18nc("@title:window", "Warning - Unverified Software");
    QString text = i18ncp("@label",
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
    qDebug() << "new status" << status;
    switch (status) {
    case QApt::SetupStatus:
    case QApt::WaitingStatus:
        progressBar()->setMaximum(0);
        setLabelText(i18nc("@label Progress bar label when waiting to start",
                           "Waiting to start."));
        break;
    case QApt::AuthenticationStatus:
        progressBar()->setMaximum(0);
        setLabelText(i18nc("@label Status label when waiting for a password",
                           "Waiting for authentication."));
        break;
    case QApt::WaitingMediumStatus:
        progressBar()->setMaximum(0);
        setLabelText(i18nc("@label Status label when waiting for a CD-ROM",
                           "Waiting for required media."));
        break;
    case QApt::WaitingLockStatus:
        progressBar()->setMaximum(0);
        setLabelText(i18nc("@label Status label",
                           "Waiting for other package managers to quit."));
        break;
    case QApt::RunningStatus:
        // We're ready for "real" progress now
        progressBar()->setMaximum(100);
        break;
    case QApt::LoadingCacheStatus:
        setLabelText(i18nc("@label Status label",
                           "Loading package cache."));
        break;
    case QApt::DownloadingStatus:
        if (m_trans->role() == QApt::UpdateCacheRole) {
            setWindowTitle(i18nc("@title:window", "Refreshing Package Information"));
            setLabelText(i18nc("@info:status", "Checking for new, removed or upgradeable packages"));
        } else {
            setWindowTitle(i18nc("@title:window", "Downloading"));
            setLabelText(i18ncp("@info:status",
                                "Downloading package file",
                                "Downloading package files",
                                m_packages.count()));
        }

        setButtons(KDialog::Cancel | KDialog::Details);
        setButtonFocus(KDialog::Details);
        break;
    case QApt::CommittingStatus:
        setWindowTitle(i18nc("@title:window", "Installing Packages"));
        setButtons(KDialog::Cancel);
        break;
    case QApt::FinishedStatus:
        if (m_mode == "install") {
            setWindowTitle(i18nc("@title:window", "Installation Complete"));

            if (m_trans->error() != QApt::Success) {
                setLabelText(i18nc("@label",
                                   "Package installation failed."));
            } else {
                setLabelText(i18ncp("@label",
                                    "Package successfully installed.",
                                    "Packages successfully installed.", m_packages.size()));
            }
        } else if (m_mode == "uninstall") {
            setWindowTitle(i18nc("@title:window", "Removal Complete"));

            if (m_trans->error() != QApt::Success) {
                setLabelText(i18nc("@label",
                                   "Package removal failed."));
                qDebug() << m_trans->error();
            } else {
                setLabelText(i18ncp("@label",
                                    "Package successfully uninstalled.",
                                    "Packages successfully uninstalled.", m_packages.size()));
            }
        } else if (m_mode == "update") {
            setWindowTitle(i18nc("@title:window", "Refresh Complete"));

            if (m_trans->error() != QApt::Success) {
                setLabelText(i18nc("@info:status", "Refresh failed."));
            } else {
                setLabelText(i18nc("@label", "Package information successfully refreshed."));
            }
        }

        progressBar()->setValue(100);
        m_done = true;

        m_trans->deleteLater();
        m_trans = 0;

        // Really a close button, but KProgressDialog use ButtonCode Cancel
        setButtonFocus(KDialog::Cancel);
        break;
    }
}

void QAptBatch::cancellableChanged(bool cancellable)
{
    setAllowCancel(cancellable);
}

void QAptBatch::updateProgress(int progress)
{
    if (progress == 100) {
        --progress;
    }

    if (progress > 100) {
        progressBar()->setMaximum(0);
    } else if (progress > m_lastRealProgress) {
        progressBar()->setMaximum(100);
        progressBar()->setValue(progress);
        m_lastRealProgress = progress;
    }
}

void QAptBatch::updateCommitMessage(const QString& message)
{
    setLabelText(message);
}
