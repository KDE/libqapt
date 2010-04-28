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

#include "cacheupdatewidget.h"

qapttest::qapttest()
    : KMainWindow()
    , m_stack(0)
{
    setWindowIcon(KIcon("application-x-deb"));

    m_backend = new QApt::Backend();
    m_backend->init();

    connect(m_backend, SIGNAL(cacheUpdateStarted()), this, SLOT(cacheUpdateStarted()));
    connect(m_backend, SIGNAL(cacheUpdateFinished()), this, SLOT(cacheUpdateFinished()));
    connect(m_backend, SIGNAL(downloadProgress(int)), this, SLOT(updateDownloadProgress(int)));
    connect(m_backend, SIGNAL(downloadMessage(int, const QString&)),
            this, SLOT(updateDownloadMessage(int, const QString&)));

    m_stack = new QStackedWidget(this);

    m_mainWidget = new QWidget(m_stack);
    QVBoxLayout *layout = new QVBoxLayout(m_mainWidget);
    m_stack->addWidget(m_mainWidget);

    m_cacheUpdateWidget = new CacheUpdateWidget(m_stack);
    m_stack->addWidget(m_cacheUpdateWidget);

    m_stack->setCurrentWidget(m_mainWidget);

    KHBox *hbox = new KHBox(m_mainWidget);
    layout->addWidget(hbox);

    m_lineEdit = new KLineEdit(hbox);
    m_lineEdit->setText("kdelibs5");
    m_lineEdit->setClearButtonShown(true);
    connect(m_lineEdit, SIGNAL(returnPressed()), this, SLOT(updateLabels()));

    QPushButton *pushButton = new QPushButton(hbox);
    pushButton->setText(i18n("Update Listing"));
    pushButton->setIcon(KIcon("system-software-update"));
    connect(pushButton, SIGNAL(clicked()), this, SLOT(updateLabels()));
    QPushButton *cachePushButton = new QPushButton(hbox);
    cachePushButton->setIcon(KIcon("dialog-password"));
    cachePushButton->setText(i18n("Update Software Lists"));
    connect(cachePushButton, SIGNAL(clicked()), this, SLOT(updateCache()));

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

    // Package count and installed package count in the statusbar
    m_packageCountLabel = new QLabel(this);
    m_packageCountLabel->setText(i18np("%1 package available",
                                     "%1 packages available",
                                     m_backend->packageCount()));

    m_installedCountLabel = new QLabel(this);
    m_installedCountLabel->setText(i18np("(%1 package installed)",
                                     "(%1 packages installed)",
                                     // Yay for flags!
                                     m_backend->packageCount(QApt::Package::Installed)));

    statusBar()->addWidget(m_packageCountLabel);
    statusBar()->addWidget(m_installedCountLabel);
    statusBar()->show();

    updateLabels();
    setCentralWidget(m_stack);

    // Lists all packages in the KDE section via kDebug(), uncomment to see in Konsole
//     m_group = m_backend->group("kde");
//     QApt::Package::List packageList = m_group->packages();
//     foreach (QApt::Package *package, packageList) {
//             kDebug() << package->name();
//     }
}

qapttest::~qapttest()
{
}

void qapttest::updateLabels()
{
    m_package = m_backend->package(m_lineEdit->text());

    // Gotta be careful when getting a package directly from the user's input. We can't currently
    // return empty Package containers when the package doesn't exist. And this is why most
    // package managers are MVC based. ;-)
    if (!m_package == 0) {
        m_nameLabel->setText(i18n("<b>Package:</b> %1", m_package->name()));
        m_sectionLabel->setText(i18n("<b>Section:</b> %1", m_package->section()));
        m_originLabel->setText(i18n("<b>Origin:</b> %1", m_package->origin()));
        QString installedSize(KGlobal::locale()->formatByteSize(m_package->installedSize()));
        m_installedSizeLabel->setText(i18n("<b>Installed Size:</b> %1", installedSize));
        m_maintainerLabel->setText(i18n("<b>Maintainer:</b> %1", m_package->maintainer()));
        m_sourceLabel->setText(i18n("<b>Source package:</b> %1", m_package->sourcePackage()));
        m_versionLabel->setText(i18n("<b>Version:</b> %1", m_package->version()));
        QString packageSize(KGlobal::locale()->formatByteSize(m_package->downloadSize()));
        m_packageSizeLabel->setText(i18n("<b>Download size:</b> %1", packageSize));
        m_shortDescriptionLabel->setText(i18n("<b>Description:</b> %1", m_package->shortDescription()));
        m_longDescriptionLabel->setText(m_package->longDescription());
    }

    if (m_package->name() == "kpat") {
        m_package->setInstall();

        m_backend->markPackagesForUpgrade();
        m_backend->commitChanges();
    }

    // Uncomment these to see the results in Konsole; I was too lazy to make a GUI for them

    // kDebug() << "============= New package Listing =============";
    // QStringList requiredByList(m_package->requiredByList());
    // foreach (const QString &name, requiredByList) {
        // kDebug() << "reverse dependency: " << name;
    // }

    // QApt::Package::List  upgradeablePackages = m_backend->upgradeablePackages();
    // foreach (QApt::Package *package, upgradeablePackages) {
            // kDebug() << "Upgradeable packages: " << package->name();
    // }

    // A convenient way to check the install status of a package
    // if (m_package->isInstalled()) {
        // kDebug() << "Package is installed!!!";
    // }

    // Another flag usage example
    // int state = m_package->state();
    // if (state & QApt::Package::Upgradeable) {
        // kDebug() << "Package is upgradeable!!!";
    // }

}

void qapttest::updateCache()
{
    m_backend->updateCache();
}

void qapttest::cacheUpdateStarted()
{
    m_cacheUpdateWidget->clear();
    m_stack->setCurrentWidget(m_cacheUpdateWidget);
    connect(m_cacheUpdateWidget, SIGNAL(cancelCacheUpdate()), m_backend, SLOT(cancelCacheUpdate()));
}

void qapttest::updateDownloadProgress(int percentage)
{
    m_cacheUpdateWidget->setTotalProgress(percentage);
}

void qapttest::updateDownloadMessage(int flag, const QString &message)
{
    QString fullMessage;

    switch(flag) {
      case QApt::Globals::DownloadFetch:
          fullMessage = i18n("Downloading: %1", message);
          break;
      case QApt::Globals::HitFetch:
          fullMessage = i18n("Checking: %1", message);
          break;
      case QApt::Globals::IgnoredFetch:
          fullMessage = i18n("Ignored: %1", message);
          break;
      default:
          fullMessage = message;
    }
    m_cacheUpdateWidget->addItem(fullMessage);
}

void qapttest::cacheUpdateFinished()
{
    m_packageCountLabel->setText(i18np("%1 package available",
                                     "%1 packages available",
                                     m_backend->packageCount()));

    m_installedCountLabel->setText(i18np("(%1 package installed)",
                                     "(%1 packages installed)",
                                     // Yay for flags!
                                     m_backend->packageCount(QApt::Package::Installed)));
    m_stack->setCurrentWidget(m_mainWidget);
}

#include "qapttest.moc"
