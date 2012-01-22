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

#include "qapttest.h"

#include <QtCore/QDir>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QStackedWidget>

#include <KDebug>
#include <KIcon>
#include <KGlobal>
#include <KLocale>
#include <KLineEdit>
#include <KStatusBar>
#include <KVBox>

#include <libqapt/debfile.h>

#include <DebconfGui.h>

#include "cacheupdatewidget.h"
#include "commitwidget.h"

QAptTest::QAptTest()
    : KMainWindow()
    , m_stack(0)
{
    setWindowIcon(KIcon("application-x-deb"));

    m_backend = new QApt::Backend();
    m_backend->init();

    connect(m_backend, SIGNAL(packageChanged()), this, SLOT(updateStatusBar()));
    connect(m_backend, SIGNAL(workerEvent(QApt::WorkerEvent)), this, SLOT(workerEvent(QApt::WorkerEvent)));
    connect(m_backend, SIGNAL(downloadProgress(int,int,int)), this, SLOT(updateDownloadProgress(int,int,int)));
    connect(m_backend, SIGNAL(downloadMessage(int,QString)),
            this, SLOT(updateDownloadMessage(int,QString)));
    connect(m_backend, SIGNAL(commitProgress(QString,int)),
            this, SLOT(updateCommitProgress(QString,int)));

    m_stack = new QStackedWidget(this);

    m_mainWidget = new QWidget(m_stack);
    QVBoxLayout *layout = new QVBoxLayout(m_mainWidget);
    m_stack->addWidget(m_mainWidget);

    m_cacheUpdateWidget = new CacheUpdateWidget(m_stack);
    m_stack->addWidget(m_cacheUpdateWidget);
    m_commitWidget = new CommitWidget(m_stack);
    m_stack->addWidget(m_commitWidget);

    m_stack->setCurrentWidget(m_mainWidget);

    KHBox *topHBox = new KHBox(m_mainWidget);
    layout->addWidget(topHBox);

    m_lineEdit = new KLineEdit(topHBox);
    m_lineEdit->setText("kdelibs5");
    m_lineEdit->setClearButtonShown(true);
    connect(m_lineEdit, SIGNAL(returnPressed()), this, SLOT(updateLabels()));

    QPushButton *showButton = new QPushButton(topHBox);
    showButton->setText(i18n("Show"));
    showButton->setIcon(KIcon("layer-visible-on"));
    connect(showButton, SIGNAL(clicked()), this, SLOT(updateLabels()));

    m_actionButton = new QPushButton(topHBox);
    connect(m_actionButton, SIGNAL(clicked()), this, SLOT(commitAction()));

    KVBox *vbox = new KVBox(m_mainWidget);
    layout->addWidget(vbox);

    m_nameLabel = new QLabel(vbox);
    m_sectionLabel = new QLabel(vbox);
    m_originLabel = new QLabel(vbox);
    m_installedSizeLabel = new QLabel(vbox);
    m_maintainerLabel = new QLabel(vbox);
    m_sourceLabel = new QLabel(vbox);
    m_versionLabel = new QLabel(vbox);
    m_packageSizeLabel = new QLabel(vbox);
    m_shortDescriptionLabel = new QLabel(vbox);
    m_longDescriptionLabel = new QLabel(vbox);

    KHBox *bottomHBox = new KHBox(m_mainWidget);
    layout->addWidget(bottomHBox);

    QPushButton *cacheButton = new QPushButton(bottomHBox);
    cacheButton->setText(i18n("Update Cache"));
    cacheButton->setIcon(KIcon("view-refresh"));
    connect(cacheButton, SIGNAL(clicked()), this, SLOT(updateCache()));

    QPushButton *upgradeButton = new QPushButton(bottomHBox);
    upgradeButton->setText(i18n("Upgrade System"));
    upgradeButton->setIcon(KIcon("system-software-update"));
    connect(upgradeButton, SIGNAL(clicked()), this, SLOT(upgrade()));

    // Package count labels in the statusbar
    m_packageCountLabel = new QLabel(this);
    m_changedPackagesLabel = new QLabel(this);
    statusBar()->addWidget(m_packageCountLabel);
    statusBar()->addWidget(m_changedPackagesLabel);
    statusBar()->show();

    updateLabels();
    updateStatusBar();
    setCentralWidget(m_stack);

    // Lists all packages in the KDE section via kDebug(), uncomment to see in Konsole
//     m_group = m_backend->group("kde");
//     QApt::PackageList packageList = m_group->packages();
//     foreach (QApt::Package *package, packageList) {
//             kDebug() << package->name();
//     }
}

QAptTest::~QAptTest()
{
}

void QAptTest::updateLabels()
{
    m_package = m_backend->package(m_lineEdit->text());

    // Gotta be careful when getting a package directly from the user's input. We can't currently
    // return empty Package containers when the package doesn't exist. And this is why most
    // package managers are MVC based. ;-)
    if (!m_package == 0) {
        m_nameLabel->setText(i18n("<b>Package:</b> %1", m_package->name()));
        m_sectionLabel->setText(i18n("<b>Section:</b> %1", m_package->section()));
        m_originLabel->setText(i18n("<b>Origin:</b> %1", m_package->origin()));
        QString installedSize(KGlobal::locale()->formatByteSize(m_package->availableInstalledSize()));
        m_installedSizeLabel->setText(i18n("<b>Installed Size:</b> %1", installedSize));
        m_maintainerLabel->setText(i18n("<b>Maintainer:</b> %1", m_package->maintainer()));
        m_sourceLabel->setText(i18n("<b>Source package:</b> %1", m_package->sourcePackage()));
        m_versionLabel->setText(i18n("<b>Version:</b> %1", m_package->version()));
        QString packageSize(KGlobal::locale()->formatByteSize(m_package->downloadSize()));
        m_packageSizeLabel->setText(i18n("<b>Download size:</b> %1", packageSize));
        m_shortDescriptionLabel->setText(i18n("<b>Description:</b> %1", m_package->shortDescription()));
        m_longDescriptionLabel->setText(m_package->longDescription());

        if (!m_package->isInstalled()) {
            m_actionButton->setText("Install Package");
            m_actionButton->setIcon(KIcon("list-add"));
        } else {
            m_actionButton->setText("Remove Package");
            m_actionButton->setIcon(KIcon("list-remove"));
        }

        if (m_package->state() & QApt::Package::Upgradeable) {
            m_actionButton->setText("Upgrade Package");
            m_actionButton->setIcon(KIcon("system-software-update"));
        }

//         kDebug() << m_package->changelogUrl();
//         kDebug() << m_package->screenshotUrl(QApt::Thumbnail);
        kDebug() << m_package->supportedUntil();
        QApt::PackageList searchList = m_backend->search("kdelibs5");
        foreach (QApt::Package *pkg, searchList) {
            kDebug() << pkg->name();
        }
    }

    // Uncomment these to see the results in Konsole; I was too lazy to make a GUI for them

    // kDebug() << "============= New package Listing =============";
    // QStringList requiredByList(m_package->requiredByList());
    // foreach (const QString &name, requiredByList) {
        // kDebug() << "reverse dependency: " << name;
    // }

    // A convenient way to check the install status of a package
    // if (m_package->isInstalled()) {
        // kDebug() << "Package is installed!!!";
    // }

}

void QAptTest::updateCache()
{
    m_backend->updateCache();
}

void QAptTest::upgrade()
{
    m_backend->markPackagesForDistUpgrade();
    m_backend->commitChanges();
}

void QAptTest::commitAction()
{
    if (!m_package->isInstalled()) {
        m_package->setInstall();
    } else {
        m_package->setRemove();
    }

    if (m_package->state() & QApt::Package::Upgradeable) {
        m_package->setInstall();
    }

    m_debconfGui = new DebconfKde::DebconfGui("/tmp/qapt-sock");
    m_debconfGui->connect(m_debconfGui, SIGNAL(activated()), m_debconfGui, SLOT(show()));
    m_debconfGui->connect(m_debconfGui, SIGNAL(deactivated()), m_debconfGui, SLOT(hide()));

    m_backend->commitChanges();
}

void QAptTest::workerEvent(QApt::WorkerEvent event)
{
    switch (event) {
        case QApt::CacheUpdateStarted:
            m_cacheUpdateWidget->clear();
            m_cacheUpdateWidget->setHeaderText(i18n("<b>Updating software sources</b>"));
            m_stack->setCurrentWidget(m_cacheUpdateWidget);
            connect(m_cacheUpdateWidget, SIGNAL(cancelCacheUpdate()), m_backend, SLOT(cancelDownload()));
            break;
        case QApt::CacheUpdateFinished:
            updateStatusBar();
            m_stack->setCurrentWidget(m_mainWidget);
            break;
        case QApt::PackageDownloadStarted:
            m_cacheUpdateWidget->clear();
            m_cacheUpdateWidget->setHeaderText(i18n("<b>Downloading Packages</b>"));
            m_stack->setCurrentWidget(m_cacheUpdateWidget);
            connect(m_cacheUpdateWidget, SIGNAL(cancelCacheUpdate()), m_backend, SLOT(cancelDownload()));
            break;
        case QApt::PackageDownloadFinished:
            updateStatusBar();
            m_stack->setCurrentWidget(m_mainWidget);
            break;
        case QApt::CommitChangesStarted:
            m_commitWidget->clear();
            m_stack->setCurrentWidget(m_commitWidget);
            break;
        case QApt::CommitChangesFinished:
            updateStatusBar();
            m_stack->setCurrentWidget(m_mainWidget);
            break;
    }
}

void QAptTest::updateDownloadProgress(int percentage, int speed, int ETA)
{
    m_cacheUpdateWidget->setTotalProgress(percentage, speed, ETA);
}

void QAptTest::updateDownloadMessage(int flag, const QString &message)
{
    QString fullMessage;

    switch (flag) {
      case QApt::DownloadFetch:
          fullMessage = i18n("Downloading: %1", message);
          break;
      case QApt::HitFetch:
          fullMessage = i18n("Checking: %1", message);
          break;
      case QApt::IgnoredFetch:
          fullMessage = i18n("Ignored: %1", message);
          break;
      default:
          fullMessage = message;
    }
    m_cacheUpdateWidget->addItem(fullMessage);
}

void QAptTest::updateCommitProgress(const QString& message, int percentage)
{
    m_commitWidget->setLabelText(message);
    m_commitWidget->setProgress(percentage);
    qDebug() << message;
}

void QAptTest::updateStatusBar()
{
    m_packageCountLabel->setText(i18n("%1 Installed, %2 upgradeable, %3 available",
                                      m_backend->packageCount(QApt::Package::Installed),
                                      m_backend->packageCount(QApt::Package::Upgradeable),
                                      m_backend->packageCount()));

    m_changedPackagesLabel->setText(i18n("%1 To install, %2 to upgrade, %3 to remove",
                                         m_backend->packageCount(QApt::Package::ToInstall),
                                         m_backend->packageCount(QApt::Package::ToUpgrade),
                                         m_backend->packageCount(QApt::Package::ToRemove)));
}

#include "qapttest.moc"
