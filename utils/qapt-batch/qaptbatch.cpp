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

#include <libqapt/globals.h>
#include <libqapt/package.h>

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

    kDebug() << m_mode;

    if (m_mode == "install") {
        commitChanges(QApt::Package::ToInstall);
    } else if (m_mode == "uninstall") {
        commitChanges(QApt::Package::ToRemove);
    } else if (m_mode == "update") {
        QDBusMessage message;
        message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
                  "/",
                  "org.kubuntu.qaptworker",
                  QLatin1String("updateCache"));
        QDBusConnection::systemBus().asyncCall(message);
    }

    if (winId != 0) {
        KWindowSystem::setMainWindow(this, winId);
    }

    setWindowIcon(KIcon("applications-other"));
    setAutoClose(false);
}

QAptBatch::~QAptBatch()
{
}

void QAptBatch::commitChanges(int mode)
{
    QMap<QString, QVariant> instructionList;

    foreach (const QString &package, m_packages) {
        instructionList.insert(package, mode);
    }

    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
              "/",
              "org.kubuntu.qaptworker",
              QLatin1String("commitChanges"));

    QList<QVariant> args;
    args << QVariant(instructionList);
    message.setArguments(args);
    QDBusConnection::systemBus().asyncCall(message);
}

void QAptBatch::cancelDownload()
{
    QDBusMessage message;
    message = QDBusMessage::createMethodCall("org.kubuntu.qaptworker",
              "/",
              "org.kubuntu.qaptworker",
              QLatin1String("cancelDownload"));
    QDBusConnection::systemBus().asyncCall(message);
}

void QAptBatch::workerStarted()
{
    kDebug() << "worker started";
    connect(m_watcher, SIGNAL(serviceOwnerChanged(QString,QString,QString)),
            this, SLOT(serviceOwnerChanged(QString, QString, QString)));

    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadProgress", this, SLOT(updateDownloadProgress(int, int, int)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "downloadMessage", this, SLOT(updateDownloadMessage(int, const QString&)));
    QDBusConnection::systemBus().connect("org.kubuntu.qaptworker", "/", "org.kubuntu.qaptworker",
                                "commitProgress", this, SLOT(updateCommitProgress(const QString&, int)));
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
            kDebug() << "downloading packages";
            connect(this, SIGNAL(cancelClicked()), this, SLOT(cancelDownload()));
            setWindowTitle(i18n("Downloading Packages"));
            setLabelText(i18n("Downloading packages"));
            show();
            break;
        case QApt::Globals::PackageDownloadFinished:
            setAllowCancel(false);
            disconnect(this, SIGNAL(cancelClicked()), this, SLOT(cancelDownload()));
            break;
        case QApt::Globals::CommitChangesStarted:
            setWindowTitle(i18n("Installing Packages"));
            show(); // In case no download was necessary
            break;
        case QApt::Globals::CommitChangesFinished:
            if (m_mode == "install") {
                setLabelText(i18np("Package successfully installed", "Packages successfully installed", m_packages.size()));
            } else if (m_mode == "uninstall") {
                setLabelText(i18np("Package successfully uninstalled", "Packages successfully uninstalled", m_packages.size()));
            }
            progressBar()->setValue(100);
            break;
    }
}

void QAptBatch::workerFinished(bool success)
{
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
        KMessageBox::sorry(0, i18n("It appears that the QApt worker has either crashed "
                                   "or disappeared. Please report a bug to the QApt maintainers"),
                           i18n("Unexpected error"));
        close();
    }
}

void QAptBatch::updateDownloadProgress(int percentage, int speed, int ETA)
{
    QString downloadSpeed = KGlobal::locale()->formatByteSize(speed);

    setLabelText(i18n("Downloading package information %1/s", downloadSpeed));
    progressBar()->setValue(percentage);
    //TODO: ETA
}

void QAptBatch::updateCommitProgress(const QString& message, int percentage)
{
    //FIXME: Percentage busted in libqapt
    //progressBar()->setValue(percentage);
    progressBar()->setValue(0);
    setLabelText(message);
}

#include "qaptbatch.moc"
