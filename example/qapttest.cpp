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

#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>

#include <KDebug>
#include <KIcon>
#include <KGlobal>
#include <KLocale>
#include <KLineEdit>
#include <KProtocolManager>
#include <KStatusBar>
#include <KVBox>

#include <LibQApt/Backend>
#include <LibQApt/Transaction>

#include <DebconfGui.h>

#include "cacheupdatewidget.h"
#include "commitwidget.h"

QAptTest::QAptTest()
    : KMainWindow()
    , m_trans(0)
    , m_stack(0)
{
    setWindowIcon(KIcon("application-x-deb"));

    m_backend = new QApt::Backend(this);
    m_backend->init();

    connect(m_backend, SIGNAL(packageChanged()), this, SLOT(updateStatusBar()));

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
    m_lineEdit->setText("muon");
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

    m_debconfGui = new DebconfKde::DebconfGui("/tmp/qapt-sock");
    m_debconfGui->connect(m_debconfGui, SIGNAL(activated()), m_debconfGui, SLOT(show()));
    m_debconfGui->connect(m_debconfGui, SIGNAL(deactivated()), m_debconfGui, SLOT(hide()));
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
        QString installedSize(KFormat().formatByteSize(m_package->availableInstalledSize()));
        m_installedSizeLabel->setText(i18n("<b>Installed Size:</b> %1", installedSize));
        m_maintainerLabel->setText(i18n("<b>Maintainer:</b> %1", m_package->maintainer()));
        m_sourceLabel->setText(i18n("<b>Source package:</b> %1", m_package->sourcePackage()));
        m_versionLabel->setText(i18n("<b>Version:</b> %1", m_package->version()));
        QString packageSize(KFormat().formatByteSize(m_package->downloadSize()));
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
    if (m_trans) // Transaction running, you could queue these though
        return;

    m_trans = m_backend->updateCache();

    // Provide proxy/locale to the transaction
    if (KProtocolManager::proxyType() == KProtocolManager::ManualProxy) {
        m_trans->setProxy(KProtocolManager::proxyFor("http"));
    }

    m_trans->setLocale(QLatin1String(setlocale(LC_MESSAGES, 0)));

    // Pass the new current transaction on to our child widgets
    m_cacheUpdateWidget->setTransaction(m_trans);
    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(onTransactionStatusChanged(QApt::TransactionStatus)));

    m_trans->run();
}

void QAptTest::upgrade()
{
    if (m_trans) // Transaction running, you could queue these though
        return;

    m_debconfGui = new DebconfKde::DebconfGui("/tmp/qapt-sock");
    m_debconfGui->connect(m_debconfGui, SIGNAL(activated()), m_debconfGui, SLOT(show()));
    m_debconfGui->connect(m_debconfGui, SIGNAL(deactivated()), m_debconfGui, SLOT(hide()));

    m_trans = m_backend->upgradeSystem(QApt::FullUpgrade);

    // Provide proxy/locale to the transaction
    if (KProtocolManager::proxyType() == KProtocolManager::ManualProxy) {
        m_trans->setProxy(KProtocolManager::proxyFor("http"));
    }

    m_trans->setLocale(QLatin1String(setlocale(LC_MESSAGES, 0)));

    // Pass the new current transaction on to our child widgets
    m_cacheUpdateWidget->setTransaction(m_trans);
    m_commitWidget->setTransaction(m_trans);
    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(onTransactionStatusChanged(QApt::TransactionStatus)));

    m_trans->run();
}

void QAptTest::commitAction()
{
    if (m_trans) // Transaction running, you could queue these though
        return;

    if (!m_package->isInstalled()) {
        m_package->setInstall();
    } else {
        m_package->setRemove();
    }

    if (m_package->state() & QApt::Package::Upgradeable) {
        m_package->setInstall();
    }

    m_trans = m_backend->commitChanges();

    // Provide proxy/locale to the transaction
    if (KProtocolManager::proxyType() == KProtocolManager::ManualProxy) {
        m_trans->setProxy(KProtocolManager::proxyFor("http"));
    }

    m_trans->setLocale(QLatin1String(setlocale(LC_MESSAGES, 0)));

    // Pass the new current transaction on to our child widgets
    m_cacheUpdateWidget->setTransaction(m_trans);
    m_commitWidget->setTransaction(m_trans);
    connect(m_trans, SIGNAL(statusChanged(QApt::TransactionStatus)),
            this, SLOT(onTransactionStatusChanged(QApt::TransactionStatus)));

    m_trans->run();
}

void QAptTest::onTransactionStatusChanged(QApt::TransactionStatus status)
{
    QString headerText;

    switch (status) {
    case QApt::RunningStatus:
        // For roles that start by downloading something, switch to download view
        if (m_trans->role() == (QApt::UpdateCacheRole || QApt::UpgradeSystemRole ||
                                QApt::CommitChangesRole || QApt::DownloadArchivesRole ||
                                QApt::InstallFileRole)) {
            m_stack->setCurrentWidget(m_cacheUpdateWidget);
        }
        break;
    case QApt::DownloadingStatus:
        m_stack->setCurrentWidget(m_cacheUpdateWidget);
        break;
    case QApt::CommittingStatus:
        m_stack->setCurrentWidget(m_commitWidget);
        break;
    case QApt::FinishedStatus:
        // FIXME: Determine which transactions need to reload cache on completion
        m_backend->reloadCache();
        m_stack->setCurrentWidget(m_mainWidget);
        updateStatusBar();

        // Clean up transaction object
        m_trans->deleteLater();
        m_trans = 0;
        delete m_debconfGui;
        m_debconfGui = 0;
        break;
    default:
        break;
    }
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
