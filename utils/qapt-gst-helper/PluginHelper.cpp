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

#include "PluginHelper.h"

#include <QCoreApplication>
#include <QtCore/QStringBuilder>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <KApplication>
#include <KIcon>
#include <KLocale>
#include <KMessageBox>
#include <KWindowSystem>
#include <QDebug>

#include "../../src/backend.h"

#include "PluginFinder.h"
#include "PluginInfo.h"

// Because qapp->exit only ends event processing the current function would not
// end, hence it is necessary to ensure that the function returns immediately.
// Also ensure that the finderthread's eventloop gets stopped to prevent
// crashes from access to implicitly shared QString data.
#define tExit(x) m_finder->stop(); m_finderThread->quit(); m_finderThread->wait(); qApp->exit(x); return;

PluginHelper::PluginHelper(QWidget *parent, const QStringList &gstDetails, int winId)
    : KProgressDialog(parent)
    , m_backend(new QApt::Backend())
    , m_winId(winId)
    , m_partialFound(false)
    , m_done(false)
    , m_details(gstDetails)
{
    connect(m_backend, SIGNAL(workerEvent(QApt::WorkerEvent)),
            this, SLOT(workerEvent(QApt::WorkerEvent)));
    connect(m_backend, SIGNAL(errorOccurred(QApt::ErrorCode,QVariantMap)),
            this, SLOT(errorOccurred(QApt::ErrorCode,QVariantMap)));
    connect(m_backend, SIGNAL(warningOccurred(QApt::WarningCode,QVariantMap)),
            this, SLOT(warningOccurred(QApt::WarningCode,QVariantMap)));
    connect(m_backend, SIGNAL(questionOccurred(QApt::WorkerQuestion,QVariantMap)),
            this, SLOT(questionOccurred(QApt::WorkerQuestion,QVariantMap)));
    connect(m_backend, SIGNAL(downloadProgress(int,int,int)),
            this, SLOT(updateDownloadProgress(int,int,int)));
    connect(m_backend, SIGNAL(commitProgress(QString,int)),
            this, SLOT(updateCommitProgress(QString,int)));

    foreach (const QString &plugin, gstDetails) {
        PluginInfo *pluginInfo = new PluginInfo(plugin);
        if (pluginInfo->isValid()) {
            m_searchList << pluginInfo;
        }
    }

    if (m_winId) {
        KWindowSystem::setMainWindow(this, m_winId);
    }
}

void PluginHelper::run()
{
    if (!m_searchList.size()) {
        KMessageBox::error(this, i18nc("@info Error message", "No valid plugin "
                                       "info was provided, so no plugins could "
                                       "be found."),
                           i18nc("@title:window", "Couldn't Find Plugins"));
        exit(ERR_RANDOM_ERR);
    }

    canSearch();

    setLabelText(i18nc("@info:progress", "Looking for plugins"));
    progressBar()->setMaximum(m_searchList.count());
    incrementProgress();
    show();

    if (!m_backend->init()) {
        // TODO: Report some sort of init error
        exit(ERR_RANDOM_ERR);
    }

    m_finder = new PluginFinder(0, m_backend);
    connect(m_finder, SIGNAL(foundCodec(QApt::Package*)),
            this, SLOT(foundCodec(QApt::Package*)));
    connect(m_finder, SIGNAL(notFound()),
            this, SLOT(notFound()));

    m_finderThread = new QThread(this);
    connect(m_finderThread, SIGNAL(started()), m_finder, SLOT(startSearch()));

    m_finder->moveToThread(m_finderThread);
    m_finder->setSearchList(m_searchList);
    m_finderThread->start();
}

PluginHelper::~PluginHelper()
{
    delete m_backend;
}

void PluginHelper::canSearch()
{
    int ret = KMessageBox::No;
    QStringList niceNames;

    foreach (PluginInfo *pluginInfo, m_searchList) {
        niceNames << pluginInfo->name();
    }

    QString message = i18np("The following plugin is required: <ul><li>%2</li></ul>"
                            "Do you want to search for this now?",
                            "The following plugins are required: <ul><li>%2</li></ul>"
                            "Do you want to search for these now?",
                            niceNames.size(),
                            niceNames.join("</li><li>"));

    // Dunno if it's possible to have both an encoder and a decoder in the same list
    int type = m_searchList.at(0)->pluginType();
    // Or if a list can have multiple requesting apps
    QString appName;
    QString requestedBy = m_searchList.at(0)->requestedBy();
    if (requestedBy.isEmpty()) {
        appName = i18nc("A program for which we have no name", "A program");
    } else {
        appName = requestedBy;
    }

    QString title;

    switch (type) {
    case PluginInfo::Encoder:
        title = i18np("%2 requires an additional plugin to encode this file",
                      "%2 requires additional plugins to encode this file",
                      m_searchList.size(),
                      appName);
        break;
    case PluginInfo::Decoder:
        title = i18np("%2 requires an additional plugin to decode this file",
                      "%2 requires additional plugins to decode this file",
                      m_searchList.size(),
                      appName);
        break;
    case PluginInfo::InvalidType:
    default:
        break;
    }

    QString msg = QLatin1Literal("<h3>") % title % QLatin1Literal("</h3>") % message;
    KGuiItem searchButton = KStandardGuiItem::yes();
    searchButton.setText(i18nc("Search for packages" ,"Search"));
    searchButton.setIcon(KIcon("edit-find"));
    ret = KMessageBox::questionYesNoWId(m_winId, msg, title, searchButton);

    if (ret != KMessageBox::Yes) {
        reject();
    }
}

void PluginHelper::offerInstallPackages()
{
    int ret = KMessageBox::No;

    foreach (QApt::Package *package, m_foundCodecs) {
        package->setInstall();
    }

    if (m_backend->markedPackages().size() == 0)
        notFoundError();

    QStringList nameList;

    foreach (QApt::Package *package, m_backend->markedPackages()) {
        nameList << package->name();
    }

    QString title = i18n("Confirm Changes");
    QString msg = i18np("Install the following package?",
                        "Install the following packages?",
                        nameList.size());

    KGuiItem installButton = KStandardGuiItem::yes();
    installButton.setText(i18nc("Install packages" ,"Install"));
    installButton.setIcon(KIcon("download"));

    ret = KMessageBox::questionYesNoListWId(m_winId, msg, nameList, title,
                                            installButton, KStandardGuiItem::no());

    if (ret != KMessageBox::Yes) {
        tExit(ERR_CANCEL);
    } else {
        install();
    }
}

void PluginHelper::errorOccurred(QApt::ErrorCode code, const QVariantMap &args)
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
            KMessageBox::detailedErrorWId(m_winId, text, details, title);
            // TODO: Report some sort of init error
            tExit(ERR_RANDOM_ERR);
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
            KMessageBox::errorWId(m_winId, text, title);
            tExit(ERR_RANDOM_ERR);
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
            KMessageBox::errorListWId(m_winId, text, untrustedItems, title);
            tExit(ERR_RANDOM_ERR);
            break;
        }
        case QApt::NotFoundError: {
            QString notFoundString = args["NotFoundString"].toString();
            text = i18nc("@label",
                        "The package \"%1\" has not been found among your software sources. "
                        "Therefore, it cannot be installed. ",
                        notFoundString);
            title = i18nc("@title:window", "Package Not Found");
            KMessageBox::errorWId(m_winId, text, title);
            tExit(ERR_RANDOM_ERR);
            break;
        }
        case QApt::UserCancelError:
            tExit(ERR_CANCEL);
            break;
        case QApt::UnknownError:
            tExit(ERR_RANDOM_ERR);
            break;
        default:
            break;
    }
}

void PluginHelper::warningOccurred(QApt::WarningCode warning, const QVariantMap &args)
{
    switch (warning) {
        case QApt::SizeMismatchWarning: {
            QString text = i18nc("@label",
                                 "The size of the downloaded items did not "
                                 "equal the expected size.");
            QString title = i18nc("@title:window", "Size Mismatch");
            KMessageBox::sorryWId(m_winId, text, title);
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

void PluginHelper::questionOccurred(QApt::WorkerQuestion code, const QVariantMap &args)
{
    QVariantMap response;

    switch (code) {
        case QApt::MediaChange: {
            QString media = args["Media"].toString();
            QString drive = args["Drive"].toString();

            QString title = i18nc("@title:window", "Media Change Required");
            QString text = i18nc("@label", "Please insert %1 into <filename>%2</filename>", media, drive);

            KMessageBox::informationWId(m_winId, text, title);
            response["MediaChanged"] = true;
            m_backend->answerWorkerQuestion(response);
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

            result = KMessageBox::warningContinueCancelListWId(m_winId, text,
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
            m_backend->answerWorkerQuestion(response);

            if (!installUntrusted) {
                tExit(ERR_CANCEL);
            }
        }
        default:
            break;
    }
}

void PluginHelper::workerEvent(QApt::WorkerEvent code)
{
    switch (code) {
        case QApt::PackageDownloadStarted:
            progressBar()->setMaximum(100);
            connect(this, SIGNAL(cancelClicked()), m_backend, SLOT(cancelDownload()));
            setWindowTitle(i18nc("@title:window", "Downloading"));
            setLabelText(i18nc("@info:status", "Downloading codecs"));
            break;
        case QApt::PackageDownloadFinished:
            setAllowCancel(false);
            disconnect(this, SIGNAL(cancelClicked()), m_backend, SLOT(cancelDownload()));
            break;
        case QApt::CommitChangesStarted:
            setWindowTitle(i18nc("@title:window", "Installing Codecs"));
            setButtons(KDialog::Cancel);
            setAllowCancel(false); //Committing changes is uninterruptable (safely, that is)
            break;
        case QApt::CommitChangesFinished:
            if (m_warningStack.size() > 0) {
                showQueuedWarnings();
            }
            if (m_errorStack.size() > 0) {
                showQueuedErrors();
            }

            setWindowTitle(i18nc("@title:window", "Installation Complete"));

            if (m_errorStack.size() > 0) {
                setLabelText(i18nc("@label",
                                   "Package installation finished with errors."));
            } else {
                setLabelText(i18nc("@label",
                                    "Codecs successfully installed"));
            }
            progressBar()->setValue(100);
            // Really a close button, but KProgressDialog uses ButtonCode Cancel
            setButtonFocus(KDialog::Cancel);
            break;
        default:
            break;
    }
}

void PluginHelper::showQueuedWarnings()
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

void PluginHelper::showQueuedErrors()
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

void PluginHelper::raiseErrorMessage(const QString &text, const QString &title)
{
    KMessageBox::error(this, text, title);
    tExit(ERR_RANDOM_ERR);
}

void PluginHelper::foundCodec(QApt::Package *package)
{
    m_foundCodecs << package;
    incrementProgress();
}

void PluginHelper::notFound()
{
    m_partialFound = true;
    incrementProgress();
}

void PluginHelper::notFoundError()
{
    QString text = i18nc("@info", "No plugins could be found");
    QString title = i18nc("@title", "Plugins Not Found");
    KMessageBox::error(this, text, title);
    exit(ERR_NO_PLUGINS);
}

void PluginHelper::incrementProgress()
{
    progressBar()->setValue(progressBar()->value() + 1);
    if (progressBar()->value() == progressBar()->maximum()) {
        if (m_foundCodecs.isEmpty()) {
            notFoundError();
        }
        offerInstallPackages();
    }
}

void PluginHelper::reject()
{
    if (m_done) {
        accept();
    }

    if (m_partialFound) {
        exit(ERR_PARTIAL_SUCCESS);
    }

    exit(ERR_CANCEL);
}

void PluginHelper::install()
{
    m_backend->commitChanges();

    setLabelText(i18nc("@label Progress bar label when waiting to start", "Waiting"));
    progressBar()->setMaximum(0); // Set progress bar to indeterminate/busy
    setAutoClose(false);
    show();
}

void PluginHelper::updateDownloadProgress(int percentage, int speed, int ETA)
{
    Q_UNUSED(speed);
    Q_UNUSED(ETA);

    if (percentage == 100) {
        --percentage;
    }

    progressBar()->setValue(percentage);
}

void PluginHelper::updateCommitProgress(const QString& message, int percentage)
{
    if (percentage == 100) {
        --percentage;
    }
    progressBar()->setValue(percentage);
    setLabelText(message);
}

#include "PluginHelper.moc"
