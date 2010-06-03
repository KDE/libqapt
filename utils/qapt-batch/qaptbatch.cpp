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
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusServiceWatcher>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>

#include <KDebug>
#include <KIcon>
#include <KLocale>
#include <KMessageBox>
#include <KWindowSystem>

#include <../../src/globals.h>
#include <../../src/package.h>

QAptBatch::QAptBatch(QString mode, QStringList packages, int winId)
    : KProgressDialog()
    , m_watcher(0)
    , m_mode(mode)
    , m_packages(packages)
    , m_vbox(0)
    , m_progressLabel(0)
{
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "errorOccurred", this, SLOT(errorOccurred(int, const QVariantMap&)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "workerStarted", this, SLOT(workerStarted()));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "workerEvent", this, SLOT(workerEvent(int)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "workerFinished", this, SLOT(workerFinished(bool)));

    m_watcher = new QDBusServiceWatcher(this);
    m_watcher->setConnection(QDBusConnection::systemBus());
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    m_watcher->addWatchedService("org.kubuntu.qaptworker");

    // Delay auto-show to 10 seconds. We can't disable it entirely, and after
    // 10 seconds people may need a reminder, or something to say we haven't died
    // If auth happens before this, we will manually show when progress happens
    setMinimumDuration(10000);
    // Set this in case we auto-show before auth
    setLabelText(i18n("Waiting for authorization"));
    progressBar()->setMinimum(0);
    progressBar()->setMaximum(0);

    if (m_mode == "install") {
        commitChanges(QApt::Package::ToInstall);
    } else if (m_mode == "uninstall") {
        commitChanges(QApt::Package::ToRemove);
    } else if (m_mode == "update") {
        QList<QVariant> args;
        workerDBusCall(QLatin1String("updateCache"), args);
    }

    if (winId != 0) {
        KWindowSystem::setMainWindow(this, winId);
    }

    setAutoClose(false);
}

QAptBatch::~QAptBatch()
{
}

void QAptBatch::workerDBusCall(QLatin1String name, QList<QVariant> &args)
{
    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
              "/",
              "org.kubuntu.qaptworker",
              name);
    if (!args.isEmpty()) {
        message.setArguments(args);
    }
    QDBusConnection::systemBus().asyncCall(message);
}

void QAptBatch::commitChanges(int mode)
{
    QMap<QString, QVariant> instructionList;

    foreach (const QString &package, m_packages) {
        instructionList.insert(package, mode);
    }


    QList<QVariant> args;
    args << QVariant(instructionList);
    workerDBusCall(QLatin1String("commitChanges"), args);
}

void QAptBatch::cancelDownload()
{
    QList<QVariant> args;
    workerDBusCall(QLatin1String("cancelDownload"), args);
}

void QAptBatch::workerStarted()
{
    // Reset the progresbar's maximum to default
    progressBar()->setMaximum(100);
    connect(m_watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadProgress", this, SLOT(updateDownloadProgress(int, int, int)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadMessage", this, SLOT(updateDownloadMessage(int, const QString&)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "commitProgress", this, SLOT(updateCommitProgress(const QString&, int)));
}
void QAptBatch::errorOccurred(int code, const QVariantMap &args)
{
    QString text;
    QString title;
    QString drive;
    QString failedItem;
    QString errorText;
    switch(code) {
        case QApt::Globals::InitError:
            text = i18n("The package system could not be initialized, your "
                        "configuration may be broken.");
            title = i18n("Initialization error");
            raiseErrorMessage(text, title);
            break;
        case QApt::Globals::LockError:
            text = i18n("Another application seems to be using the package "
                        "system at this time. You must close all other package "
                        "managers before you will be able to install or remove "
                        "any packages.");
            title = i18n("Unable to obtain package system lock");
            raiseErrorMessage(text, title);
            break;
        case QApt::Globals::DiskSpaceError:
            drive = args["DirectoryString"].toString();
            text = i18n("You do not have enough disk space in the directory "
                        "at %1 to continue with this operation.", drive);
            title = i18n("Low disk space");
            raiseErrorMessage(text, title);
            break;
        case QApt::Globals::CommitError:
            failedItem = args["FailedItem"].toString();
            errorText = args["ErrorText"].toString();
            text = i18n("An error occurred while committing changes.");

            if (!failedItem.isEmpty() && !errorText.isEmpty()) {
                text.append("\n\n");
                text.append(failedItem);
                text.append('\n');
                text.append(errorText);
            }

            title = i18n("Commit error");
            raiseErrorMessage(text, title);
            break;
        case QApt::Globals::AuthError:
            text = i18n("This operation cannot continue since proper "
                        "authorization was not provided");
            title = i18n("Authentication error");
            raiseErrorMessage(text, title);
            break;
    }
}

void QAptBatch::raiseErrorMessage(const QString &text, const QString &title)
{
    KMessageBox::sorry(0, text, title);
    workerFinished(false);
    close();
}

void QAptBatch::workerEvent(int code)
{
    switch (code) {
        case QApt::Globals::CacheUpdateStarted:
            connect(this, SIGNAL(cancelClicked()), this, SLOT(cancelDownload()));
            setWindowTitle(i18n("Refreshing package information"));
            setLabelText(i18n("Downloading package information"));
            show();
            break;
        case QApt::Globals::CacheUpdateFinished:
            setLabelText(i18n("Package information successfully refreshed"));
            disconnect(this, SIGNAL(cancelClicked()), this, SLOT(cancelDownload()));
            break;
        case QApt::Globals::PackageDownloadStarted:
            connect(this, SIGNAL(cancelClicked()), this, SLOT(cancelDownload()));
            setWindowTitle(i18n("Downloading"));
            setLabelText(i18n("Downloading packages"));
            show();
            break;
        case QApt::Globals::PackageDownloadFinished:
            setAllowCancel(false);
            disconnect(this, SIGNAL(cancelClicked()), this, SLOT(cancelDownload()));
            break;
        case QApt::Globals::CommitChangesStarted:
            setWindowTitle(i18n("Installing"));
            showButton(Cancel, false); //Committing changes is uninterruptable (safely, that is)
            show(); // In case no download was necessary
            break;
        case QApt::Globals::CommitChangesFinished:
            if (m_mode == "install") {
                setWindowTitle(i18n("Installation complete"));
                setLabelText(i18np("Package successfully installed", "Packages successfully installed", m_packages.size()));
            } else if (m_mode == "uninstall") {
                setWindowTitle(i18n("Removal complete"));
                setLabelText(i18np("Package successfully uninstalled", "Packages successfully uninstalled", m_packages.size()));
            }
            progressBar()->setValue(100);
            break;
    }
}

void QAptBatch::workerFinished(bool success)
{
    Q_UNUSED(success);
    disconnect(m_watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
               this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    QDBusConnection::systemBus().disconnect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadProgress", this, SLOT(updateDownloadProgress(int, int, int)));
    QDBusConnection::systemBus().disconnect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadMessage", this, SLOT(updateDownloadMessage(int, const QString&)));
    QDBusConnection::systemBus().disconnect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "commitProgress", this, SLOT(updateCommitProgress(const QString&, int)));
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
        QString text = i18n("It appears that the QApt worker has either crashed "
                            "or disappeared. Please report a bug to the QApt maintainers");
        QString title = i18n("Unexpected error");

        raiseErrorMessage(text, title);
    }
}

void QAptBatch::updateDownloadProgress(int percentage, int speed, int ETA)
{
    QString labelText;

    QString downloadSpeed;
    if (speed != 0) {
        downloadSpeed = i18nc("Download rate", "at %1/s", KGlobal::locale()->formatByteSize(speed));
    }

    QString downloadLabel;

    if (m_mode == "update") {
        downloadLabel = i18n("Downloading package information %1", downloadSpeed);
    } else {
        downloadLabel = i18np("Downloading package file %1",
                              "Downloading package files %1",
                              downloadSpeed, m_packages.count());
    }

    progressBar()->setValue(percentage);

    QString timeRemaining;
    int ETAMilliseconds = ETA * 1000;

    // Greater than zero and less than 2 weeks
    if (ETAMilliseconds > 0 && ETAMilliseconds < 14*24*60*60) {
        timeRemaining = i18nc("Remaining time label", "\n\n%1 remaining",
                              KGlobal::locale()->prettyFormatDuration(ETAMilliseconds));
    }

    labelText = QString(downloadLabel + timeRemaining);
    setLabelText(labelText);
}

void QAptBatch::updateCommitProgress(const QString& message, int percentage)
{
    progressBar()->setValue(percentage);
    setLabelText(message);
}

#include "qaptbatch.moc"
