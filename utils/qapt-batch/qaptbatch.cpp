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

// Qt includes
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>

// KDE includes
#include <KApplication>
#include <KDebug>
#include <KIcon>
#include <KLocale>
#include <KMessageBox>
#include <KWindowSystem>

#include "detailswidget.h"
#include "../../src/globals.h"
#include "../../src/package.h"
#include "workerdbus.h"

QAptBatch::QAptBatch(QString mode, QStringList packages, int winId)
    : KProgressDialog()
    , m_worker(0)
    , m_watcher(0)
    , m_mode(mode)
    , m_packages(packages)
    , m_detailsWidget(0)
    , m_done(false)
{
    m_worker = new OrgKubuntuQaptworkerInterface("org.kubuntu.qaptworker",
                                                  "/", QDBusConnection::systemBus(),
                                                  this);
    connect(m_worker, SIGNAL(errorOccurred(int, const QVariantMap&)),
            this, SLOT(errorOccurred(int, const QVariantMap&)));
    connect(m_worker, SIGNAL(warningOccurred(int, const QVariantMap&)),
            this, SLOT(warningOccurred(int, const QVariantMap&)));
    connect(m_worker, SIGNAL(workerStarted()), this, SLOT(workerStarted()));
    connect(m_worker, SIGNAL(workerEvent(int)), this, SLOT(workerEvent(int)));
    connect(m_worker, SIGNAL(workerFinished(bool)), this, SLOT(workerFinished(bool)));

    m_watcher = new QDBusServiceWatcher(this);
    m_watcher->setConnection(QDBusConnection::systemBus());
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    m_watcher->addWatchedService("org.kubuntu.qaptworker");

    // Delay auto-show to 10 seconds. We can't disable it entirely, and after
    // 10 seconds people may need a reminder, or something to say we haven't died
    // If auth happens before this, we will manually show when progress happens
    setMinimumDuration(10000);
    // Set this in case we auto-show before auth
    setLabelText(i18nc("@label", "Waiting for authorization"));
    progressBar()->setMaximum(0); // Set progress bar to indeterminate/busy

    m_detailsWidget = new DetailsWidget(this);
    setDetailsWidget(m_detailsWidget);

    m_worker->setLocale(setlocale(LC_ALL, 0));
    if (m_mode == "install") {
        commitChanges(QApt::Package::ToInstall);
    } else if (m_mode == "uninstall") {
        commitChanges(QApt::Package::ToRemove);
    } else if (m_mode == "update") {
        m_worker->updateCache();
    }

    if (winId != 0) {
        KWindowSystem::setMainWindow(this, winId);
    }

    setAutoClose(false);
}

QAptBatch::~QAptBatch()
{
}

void QAptBatch::reject()
{
    if (m_done) {
        accept();
    } else {
        KProgressDialog::reject();
    }
}

void QAptBatch::commitChanges(int mode)
{
    QMap<QString, QVariant> instructionList;

    foreach (const QString &package, m_packages) {
        instructionList.insert(package, mode);
    }

    m_worker->commitChanges(instructionList);
}

void QAptBatch::workerStarted()
{
    // Reset the progresbar's maximum to default
    progressBar()->setMaximum(100);
    connect(m_watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    connect(m_worker, SIGNAL(questionOccurred(int, const QVariantMap&)),
            this, SLOT(questionOccurred(int, const QVariantMap&)));
    connect(m_worker, SIGNAL(downloadProgress(int, int, int)),
            this, SLOT(updateDownloadProgress(int, int, int)));
    connect(m_worker, SIGNAL(commitProgress(const QString&, int)),
            this, SLOT(updateCommitProgress(const QString&, int)));
}
void QAptBatch::errorOccurred(int code, const QVariantMap &args)
{
    QString text;
    QString title;
    QString failedItem;
    QString errorText;
    QString drive;

    switch(code) {
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
            drive = args["DirectoryString"].toString();
            text = i18nc("@label",
                         "You do not have enough disk space in the directory "
                         "at %1 to continue with this operation.", drive);
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
            m_errorStack.append(args);
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
            QStringList untrustedItems = args["UntrustedItems"].toStringList();
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
            QString notFoundString = args["NotFoundString"].toString();
            text = i18nc("@label",
                        "The package \"%1\" has not been found among your software sources. "
                        "Therefore, it cannot be installed. ",
                        notFoundString);
            title = i18nc("@title:window", "Package Not Found");
            KMessageBox::error(this, text, title);
            close();
            break;
        }
        case QApt::UserCancelError:
            // KProgressDialog handles cancel, nothing to do
        case QApt::UnknownError:
        default:
            break;
    }
}

void QAptBatch::warningOccurred(int warning, const QVariantMap &args)
{
    switch (warning) {
        case QApt::SizeMismatchWarning: {
            QString text = i18nc("@label",
                                 "The size of the downloaded items did not "
                                 "equal the expected size.");
            QString title = i18nc("@title:window", "Size Mismatch");
            KMessageBox::sorry(this, text, title);
            break;
        }
        case QApt::FetchFailedWarning: {
            m_warningStack.append(args);
            break;
        }
        case QApt::UnknownWarning:
        default:
            break;
    }

}

void QAptBatch::questionOccurred(int code, const QVariantMap &args)
{
    // show() so that closing our question dialog doesn't quit the program
    show();
    QVariantMap response;

    switch (code) {
        case QApt::MediaChange: {
            QString media = args["Media"].toString();
            QString drive = args["Drive"].toString();

            QString title = i18nc("@title:window", "Media Change Required");
            QString text = i18nc("@label", "Please insert %1 into <filename>%2</filename>", media, drive);

            KMessageBox::information(this, text, title);
            response["MediaChanged"] = true;
            m_worker->answerWorkerQuestion(response);
        }
        case QApt::InstallUntrusted: {
            QStringList untrustedItems = args["UntrustedItems"].toStringList();

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
                        untrustedItems.size());
            int result = KMessageBox::Cancel;
            bool installUntrusted = false;

            result = KMessageBox::warningContinueCancelList(this, text,
                                                            untrustedItems, title);
            switch (result) {
                case KMessageBox::Continue:
                    installUntrusted = true;
                    break;
                case KMessageBox::Cancel:
                    installUntrusted = false;
                    break;
            }

            response["InstallUntrusted"] = installUntrusted;
            m_worker->answerWorkerQuestion(response);

            if (!installUntrusted) {
                close();
            }
        }
    }
}

void QAptBatch::raiseErrorMessage(const QString &text, const QString &title)
{
    KMessageBox::error(0, text, title);
    workerFinished(false);
    close();
}

void QAptBatch::workerEvent(int code)
{
    switch (code) {
        case QApt::CacheUpdateStarted:
            connect(this, SIGNAL(cancelClicked()), m_worker, SLOT(cancelDownload()));
            setWindowTitle(i18nc("@title:window", "Refreshing Package Information"));
            setLabelText(i18nc("@info:status", "Checking for new, removed or upgradeable packages"));
            setButtons(KDialog::Cancel | KDialog::Details);
            setButtonFocus(KDialog::Details);
            show();
            break;
        case QApt::CacheUpdateFinished:
            setWindowTitle(i18nc("@title:window", "Refresh Complete"));
            if (m_warningStack.size() > 0) {
                showQueuedWarnings();
            }
            if (m_errorStack.size() > 0) {
                showQueuedErrors();
            }

            if (m_errorStack.size() > 0) {
                setLabelText(i18nc("@info:status", "Refresh completed with errors"));
            } else {
                setLabelText(i18nc("@label", "Package information successfully refreshed"));
            }
            disconnect(this, SIGNAL(cancelClicked()), m_worker, SLOT(cancelDownload()));
            progressBar()->setValue(100);
            m_detailsWidget->hide();
            setButtons(KDialog::Close);
            setButtonFocus(KDialog::Close);
            break;
        case QApt::PackageDownloadStarted:
            connect(this, SIGNAL(cancelClicked()), m_worker, SLOT(cancelDownload()));
            setWindowTitle(i18nc("@title:window", "Downloading"));
            setLabelText(i18ncp("@info:status",
                                "Downloading package file",
                                "Downloading package files",
                                m_packages.count()));
            setButtons(KDialog::Cancel | KDialog::Details);
            setButtonFocus(KDialog::Details);
            show();
            break;
        case QApt::PackageDownloadFinished:
            setAllowCancel(false);
            disconnect(this, SIGNAL(cancelClicked()), m_worker, SLOT(cancelDownload()));
            break;
        case QApt::CommitChangesStarted:
            setWindowTitle(i18nc("@title:window", "Installing Packages"));
            m_detailsWidget->hide();
            setButtons(KDialog::Cancel);
            setAllowCancel(false); //Committing changes is uninterruptable (safely, that is)
            show(); // In case no download was necessary
            break;
        case QApt::CommitChangesFinished:
            if (m_warningStack.size() > 0) {
                showQueuedWarnings();
            }
            if (m_errorStack.size() > 0) {
                showQueuedErrors();
            }
            if (m_mode == "install") {
                setWindowTitle(i18nc("@title:window", "Installation Complete"));

                if (m_errorStack.size() > 0) {
                    setLabelText(i18nc("@label",
                                       "Package installation finished with errors."));
                } else {
                    setLabelText(i18ncp("@label",
                                        "Package successfully installed",
                                        "Packages successfully installed", m_packages.size()));
                }
            } else if (m_mode == "uninstall") {
                setWindowTitle(i18nc("@title:window", "Removal Complete"));

                if (m_errorStack.size() > 0) {
                    setLabelText(i18nc("@label",
                                       "Package removal finished with errors."));
                } else {
                    setLabelText(i18ncp("@label",
                                        "Package successfully uninstalled",
                                        "Packages successfully uninstalled", m_packages.size()));
                }
            }
            progressBar()->setValue(100);
            m_done = true;
            // Really a close button, but KProgressDialog use ButtonCode Cancel
            setButtonFocus(KDialog::Cancel);
            break;
    }
}

void QAptBatch::workerFinished(bool success)
{
    Q_UNUSED(success);
    disconnect(m_watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
               this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    disconnect(m_worker, SIGNAL(downloadProgress(int, int, int)),
               this, SLOT(updateDownloadProgress(int, int, int)));
    disconnect(m_worker, SIGNAL(commitProgress(const QString&, int)),
               this, SLOT(updateCommitProgress(const QString&, int)));
}

void QAptBatch::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(name);
    if (oldOwner.isEmpty()) {
        return; // Don't care, just appearing
    }

    if (newOwner.isEmpty()) {
        // Normally we'd handle this in errorOccurred, but if the worker dies it
        // can't really tell us, can it?
        QString text = i18nc("@label", "It appears that the QApt worker has either crashed "
                            "or disappeared. Please report a bug to the QApt maintainers");
        QString title = i18nc("@title:window", "Unexpected Error");

        raiseErrorMessage(text, title);
    }
}

void QAptBatch::updateDownloadProgress(int percentage, int speed, int ETA)
{
    QString timeRemaining;
    int ETAMilliseconds = ETA * 1000;

    // Greater than zero and less than 2 weeks
    if (ETAMilliseconds > 0 && ETAMilliseconds < 14*24*60*60) {
        timeRemaining = KGlobal::locale()->prettyFormatDuration(ETAMilliseconds);
    } else {
        timeRemaining = i18nc("@info:progress Remaining time", "Unknown");
    }

    QString downloadSpeed;
    if (speed == -1) {
        downloadSpeed = i18nc("@info:progress Download rate", "Unknown");
    } else {
        downloadSpeed = i18nc("@info:progress Download rate",
                              "%1/s", KGlobal::locale()->formatByteSize(speed));
    }

    if (percentage == 100) {
        --percentage;
    }
    progressBar()->setValue(percentage);
    m_detailsWidget->setTimeText(timeRemaining);
    m_detailsWidget->setSpeedText(downloadSpeed);
}

void QAptBatch::updateCommitProgress(const QString& message, int percentage)
{
    if (percentage == 100) {
        --percentage;
    }
    progressBar()->setValue(percentage);
    setLabelText(message);
}

void QAptBatch::showQueuedWarnings()
{
    QString details;
    QString text = i18nc("@label", "Unable to download the following packages:");
    foreach (const QVariantMap &args, m_warningStack) {
        QString failedItem = args["FailedItem"].toString();
        QString warningText = args["WarningText"].toString();
        details.append(i18nc("@label",
                             "Failed to download %1\n"
                             "%2\n\n", failedItem, warningText));
    }
    QString title = i18nc("@title:window", "Some Packages Could not be Downloaded");
    KMessageBox::detailedError(this, text, details, title);
}

void QAptBatch::showQueuedErrors()
{
    QString details;
    QString text = i18ncp("@label", "An error occurred while applying changes:",
                                    "The following errors occurred while applying changes:",
                                    m_warningStack.size());
    foreach (const QVariantMap &args, m_errorStack) {
        QString failedItem = i18nc("@label Shows which package failed", "Package: %1", args["FailedItem"].toString());
        QString errorText = i18nc("@label Shows the error", "Error: %1", args["ErrorText"].toString());
        details.append(failedItem % '\n' % errorText % "\n\n");
    }

    QString title = i18nc("@title:window", "Commit error");
    KMessageBox::detailedError(this, text, details, title);
}

#include "qaptbatch.moc"
