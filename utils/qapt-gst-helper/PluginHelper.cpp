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

// Qt includes
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QStringBuilder>
#include <QThread>
#include <QTimer>
#include <QPushButton>

// KDE includes
#include <KLocalizedString>
#include <KMessageBox>
#include <KProtocolManager>
#include <KStandardGuiItem>
#include <KWindowSystem>

// LibQApt includes
#include <QApt/Backend>
#include <QApt/Config>
#include <QApt/Transaction>

// Own includes
#include "PluginFinder.h"
#include "PluginInfo.h"

// Because qapp->exit only ends event processing the current function would not
// end, hence it is necessary to ensure that the function returns immediately.
// Also ensure that the finderthread's eventloop gets stopped to prevent
// crashes from access to implicitly shared QString data.
#define tExit(x) m_finder->stop(); m_finderThread->quit(); m_finderThread->wait(); qApp->exit(x); return;

PluginHelper::PluginHelper(QWidget *parent, const QStringList &gstDetails, int winId)
    : QProgressDialog(parent)
    , m_backend(new QApt::Backend(this))
    , m_trans(nullptr)
    , m_winId(winId)
    , m_partialFound(false)
    , m_done(false)
    , m_details(gstDetails)
{
    // Set frontend capabilities
    QApt::FrontendCaps caps = (QApt::FrontendCaps)(QApt::MediumPromptCap | QApt::UntrustedPromptCap);
    m_backend->setFrontendCaps(caps);

    foreach (const QString &plugin, gstDetails) {
        PluginInfo *pluginInfo = new PluginInfo(plugin);
        if (pluginInfo->isValid()) {
            m_searchList << pluginInfo;
        }
    }

    if (m_winId) {
        KWindowSystem::setMainWindow(this, m_winId);
    }

    QPushButton *button = new QPushButton(this);
    KGuiItem::assign(button, KStandardGuiItem::cancel());
    setCancelButton(button);
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
    setMaximum(m_searchList.count());
    incrementProgress();
    show();

    if (!m_backend->init())
        initError();

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

void PluginHelper::setCloseButton()
{
    QPushButton *button = new QPushButton(this);
    KGuiItem::assign(button, KStandardGuiItem::close());
    setCancelButton(button);
}

void PluginHelper::initError()
{
    QString details = m_backend->initErrorMessage();

    QString text = i18nc("@label",
                         "The package system could not be initialized, your "
                         "configuration may be broken.");
    QString title = i18nc("@title:window", "Initialization error");

    KMessageBox::detailedError(this, text, details, title);
    exit(ERR_RANDOM_ERR);
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
    QString requestedBy = m_searchList.at(0)->requestedBy();
    QString appName = (requestedBy.isEmpty()) ?
                      i18nc("A program for which we have no name", "A program") :
                      requestedBy;

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
    searchButton.setIcon(QIcon::fromTheme("edit-find"));
    ret = KMessageBox::questionYesNoWId(m_winId, msg, title, searchButton);

    if (ret != KMessageBox::Yes) {
        reject();
    }
}

void PluginHelper::offerInstallPackages()
{
    int ret = KMessageBox::No;

    for (QApt::Package *package : m_foundCodecs) {
        package->setInstall();
    }

    if (m_backend->markedPackages().size() == 0)
        notFoundError();

    QStringList nameList;

    for (QApt::Package *package : m_backend->markedPackages()) {
        nameList << package->name();
    }

    QString title = i18n("Confirm Changes");
    QString msg = i18np("Install the following package?",
                        "Install the following packages?",
                        nameList.size());

    KGuiItem installButton = KStandardGuiItem::yes();
    installButton.setText(i18nc("Install packages" ,"Install"));
    installButton.setIcon(QIcon::fromTheme("download"));

    ret = KMessageBox::questionYesNoListWId(m_winId, msg, nameList, title,
                                            installButton, KStandardGuiItem::no());

    if (ret != KMessageBox::Yes) {
        tExit(ERR_CANCEL);
    } else {
        install();
    }
}

void PluginHelper::cancellableChanged(bool cancellable)
{
    QPushButton *button = new QPushButton(this);
    KGuiItem::assign(button, KStandardGuiItem::cancel());
    button->setEnabled(cancellable);
    setCancelButton(button);
}

void PluginHelper::transactionErrorOccurred(QApt::ErrorCode code)
{
    QString text;
    QString title;

    switch(code) {
        case QApt::InitError: {
            text = i18nc("@label",
                         "The package system could not be initialized, your "
                         "configuration may be broken.");
            title = i18nc("@title:window", "Initialization error");
            KMessageBox::detailedErrorWId(m_winId, text, m_trans->errorDetails(), title);
            // TODO: Report some sort of init error with the exit value
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
            text = i18nc("@label",
                         "You do not have enough disk space in the directory "
                         "at %1 to continue with this operation.", m_trans->errorDetails());
            title = i18nc("@title:window", "Low disk space");
            raiseErrorMessage(text, title);
            break;
        case QApt::FetchError:
            text = i18nc("@label", "Could not download packages");
            title = i18nc("@title:window", "Download failed");
            KMessageBox::detailedError(this, text, m_trans->errorDetails(), title);
            tExit(ERR_RANDOM_ERR);
            break;
        case QApt::CommitError:
            text = i18nc("@label", "An error occurred while applying changes:");
            title = i18nc("@title:window", "Commit Error");
            KMessageBox::detailedError(this, text, m_trans->errorDetails(), title);
            tExit(ERR_RANDOM_ERR);
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
            QStringList untrustedItems = m_trans->untrustedPackages();
            if (untrustedItems.size()) {
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
        case QApt::NotFoundError:
            text = i18nc("@label",
                        "The package \"%1\" has not been found among your software sources. "
                        "Therefore, it cannot be installed. ",
                        m_trans->errorDetails());
            title = i18nc("@title:window", "Package Not Found");
            KMessageBox::errorWId(m_winId, text, title);
            tExit(ERR_RANDOM_ERR);
            break;
        case QApt::UnknownError:
            tExit(ERR_RANDOM_ERR);
            break;
        default:
            break;
    }
}

void PluginHelper::provideMedium(const QString &label, const QString &mountPoint)
{
    QString title = i18nc("@title:window", "Media Change Required");
    QString text = xi18nc("@label", "Please insert %1 into <filename>%2</filename>",
                          label, mountPoint);

    KMessageBox::informationWId(m_winId, text, title);
    m_trans->provideMedium(mountPoint);
}

void PluginHelper::untrustedPrompt(const QStringList &untrustedPackages)
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
        tExit(ERR_CANCEL);
    }
}

void PluginHelper::transactionStatusChanged(QApt::TransactionStatus status)
{
    switch (status) {
    case QApt::SetupStatus:
    case QApt::WaitingStatus:
        setMaximum(0);
        setLabelText(i18nc("@label Progress bar label when waiting to start",
                           "Waiting to start."));
        break;
    case QApt::AuthenticationStatus:
        setMaximum(0);
        setLabelText(i18nc("@label Status label when waiting for a password",
                           "Waiting for authentication."));
        break;
    case QApt::WaitingMediumStatus:
        setMaximum(0);
        setLabelText(i18nc("@label Status label when waiting for a CD-ROM",
                           "Waiting for required media."));
        break;
    case QApt::WaitingLockStatus:
        setMaximum(0);
        setLabelText(i18nc("@label Status label",
                           "Waiting for other package managers to quit."));
        break;
    case QApt::RunningStatus:
        // We're ready for "real" progress now
        setMaximum(100);
        break;
    case QApt::LoadingCacheStatus:
        setLabelText(i18nc("@label Status label",
                           "Loading package cache."));
        break;
    case QApt::DownloadingStatus:
        setWindowTitle(i18nc("@title:window", "Downloading"));
        setLabelText(i18nc("@info:status", "Downloading codecs"));
        break;
    case QApt::CommittingStatus:
        setWindowTitle(i18nc("@title:window", "Installing"));
        setLabelText(i18nc("@info:status", "Installing codecs"));
        break;
    case QApt::FinishedStatus:
        if (m_trans->exitStatus() == QApt::ExitCancelled) {
            tExit(ERR_CANCEL);
        } else if (m_trans->exitStatus() != QApt::ExitSuccess) {
            setLabelText(i18nc("@label", "Package installation finished with errors."));
            setWindowTitle(i18nc("@title:window", "Installation Failed"));
        } else {
            setLabelText(i18nc("@label", "Codecs successfully installed"));
            setWindowTitle(i18nc("@title:window", "Installation Complete"));
            m_done = true;
        }

        setValue(100);
        setCloseButton();

        m_trans->deleteLater();
        m_trans = 0;
        break;
    case QApt::WaitingConfigFilePromptStatus:
    default:
        break;
    }
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
    setValue(value() + 1);
    if (value() == maximum()) {
        if (m_foundCodecs.isEmpty()) {
            notFoundError();
        }
        offerInstallPackages();
    }
}

void PluginHelper::reject()
{
    if (m_done) {
        qApp->quit();
        return;
    }

    if (m_partialFound) {
        exit(ERR_PARTIAL_SUCCESS);
    }

    exit(ERR_CANCEL);
}

void PluginHelper::install()
{
    m_trans = m_backend->commitChanges();

    // Provide proxy/locale to the transaction
    if (KProtocolManager::proxyType() == KProtocolManager::ManualProxy) {
        m_trans->setProxy(KProtocolManager::proxyFor("http"));
    }

    m_trans->setLocale(QLatin1String(setlocale(LC_MESSAGES, 0)));

    // Connect the transaction all up to our slots
    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(transactionStatusChanged(QApt::TransactionStatus)));
    connect(m_trans, SIGNAL(errorOccurred(QApt::ErrorCode)),
            this, SLOT(transactionErrorOccurred(QApt::ErrorCode)));
    connect(m_trans, SIGNAL(cancellableChanged(bool)),
            this, SLOT(cancellableChanged(bool)));
    connect(m_trans, SIGNAL(mediumRequired(QString,QString)),
            this, SLOT(provideMedium(QString,QString)));
    connect(m_trans, SIGNAL(promptUntrusted(QStringList)),
            this, SLOT(untrustedPrompt(QStringList)));
    connect(m_trans, SIGNAL(progressChanged(int)),
            this, SLOT(updateProgress(int)));
    connect(m_trans, SIGNAL(statusDetailsChanged(QString)),
            this, SLOT(updateCommitStatus(QString)));

    // Connect us to the transaction
    connect(this, SIGNAL(cancelClicked()), m_trans, SLOT(cancel()));

    m_trans->run();

    setLabelText(i18nc("@label Progress bar label when waiting to start", "Waiting"));
    setMaximum(0); // Set progress bar to indeterminate/busy
    setAutoClose(false);
    show();
}

void PluginHelper::updateProgress(int percentage)
{
    if (percentage == 100) {
        --percentage;
    }

    setValue(percentage);
}

void PluginHelper::updateCommitStatus(const QString& message)
{
    setLabelText(message);
}
