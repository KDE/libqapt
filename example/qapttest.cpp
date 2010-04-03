/*
 * qapttest.cpp
 *
 * Copyright (C) 2008 Jonathan Thomas <echidnaman@kubuntu.org>
 */
#include "qapttest.h"

#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QHBoxLayout>

#include <KDebug>
#include <KIcon>
#include <KGlobal>
#include <KLocale>
#include <KLineEdit>
#include <KStatusBar>
#include <KVBox>
#include <KProgressDialog>


qapttest::qapttest()
    : KMainWindow()
{
    setWindowIcon(KIcon("application-x-deb"));

    m_backend = new QApt::Backend();
    m_backend->init();

    connect(m_backend, SIGNAL(cacheUpdateStarted()), this, SLOT(cacheUpdateStarted()));
    connect(m_backend, SIGNAL(percentageChanged(int)), this, SLOT(percentageChanged(int)));

    m_mainWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_mainWidget);

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
    m_stateLabel = new QLabel(vbox);
    m_sectionLabel = new QLabel(vbox);
    m_installedSizeLabel = new QLabel(vbox);
    m_maintainerLabel = new QLabel(vbox);
    m_sourceLabel = new QLabel(vbox);
    m_versionLabel = new QLabel(vbox);
    m_packageSizeLabel = new QLabel(vbox);
    m_shortDescriptionLabel = new QLabel(vbox);
    m_longDescriptionLabel = new QLabel(vbox);

    updateLabels();
    setCentralWidget(m_mainWidget);

    // Package count and installed package count in the sidebar
    QLabel* packageCountLabel = new QLabel(this);

    packageCountLabel->setText(i18np("%1 package available",
                                     "%1 packages available",
                                     m_backend->packageCount()));
    QLabel* installedCountLabel = new QLabel(this);
    installedCountLabel->setText(i18np("(%1 package installed)",
                                     "(%1 packages installed)",
                                     // Yay for flags!
                                     m_backend->packageCount(QApt::Package::Installed)));
    statusBar()->addWidget(packageCountLabel);
    statusBar()->addWidget(installedCountLabel);
    statusBar()->show();

    // Lists all packages in the KDE section via kDebug()
    m_group = m_backend->group("kde");
    QApt::Package::List packageList = m_group->packages();
    foreach (QApt::Package *package, packageList) {
            kDebug() << package->name();
    }
}

qapttest::~qapttest()
{
}

void qapttest::updateLabels()
{
    kDebug() << "============= New package Listing =============";
    m_package = m_backend->package(m_lineEdit->text());

    m_nameLabel->setText(i18n("<b>Package:</b> %1", m_package->name()));
    m_stateLabel->setText(i18n("<b>State:</b> %1", m_package->state()));
    m_sectionLabel->setText(i18n("<b>Section:</b> %1", m_package->section()));
    QString installedSize(KGlobal::locale()->formatByteSize(m_package->installedSize()));
    m_installedSizeLabel->setText(i18n("<b>Installed Size:</b> %1", installedSize));
    m_maintainerLabel->setText(i18n("<b>Maintainer:</b> %1", m_package->maintainer()));
    m_sourceLabel->setText(i18n("<b>Source package:</b> %1", m_package->sourcePackage()));
    m_sourceLabel->setText(i18n("<b>Version:</b> %1", m_package->version()));
    QString packageSize(KGlobal::locale()->formatByteSize(m_package->downloadSize()));
    m_packageSizeLabel->setText(i18n("<b>Download size:</b> %1", packageSize));
    m_shortDescriptionLabel->setText(i18n("<b>Description:</b> %1", m_package->shortDescription()));
    m_longDescriptionLabel->setText(m_package->longDescription());

    QStringList requiredByList(m_package->requiredByList());
    foreach (const QString &name, requiredByList) {
        kDebug() << "reverse dependency: " << name;
    }

    QApt::Package::List  upgradeablePackages = m_backend->upgradeablePackages();
    foreach (QApt::Package *package, upgradeablePackages) {
            kDebug() << "Upgradeable packages: " << package->name();
    }

    // A convenient way to check the install status of a package
    if (m_package->isInstalled()) {
        kDebug() << "Package is installed!!!";
    }

    // Another flag usage example
    int state = m_package->state();
    if (state & QApt::Package::Upgradeable) {
        kDebug() << "Package is upgradeable!!!";
    }

}

void qapttest::updateCache()
{
    m_backend->updateCache();
}

void qapttest::cacheUpdateStarted()
{
    m_mainWidget->setEnabled(false);
    KProgressDialog *cacheUpdateDialog = new KProgressDialog(this, i18n("Updating package cache"));
    cacheUpdateDialog->progressBar()->setRange(0,0);
}

void qapttest::percentageChanged(int percentage)
{
    kDebug() << "Percentage == " << percentage;
}

#include "qapttest.moc"
