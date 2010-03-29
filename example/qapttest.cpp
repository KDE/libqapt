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


qapttest::qapttest()
    : KMainWindow()
{
    setWindowIcon(KIcon("application-x-deb"));

    m_backend = new QApt::Backend();
    m_backend->init();

    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(mainWidget);

    KHBox *hbox = new KHBox(mainWidget);
    layout->addWidget(hbox);

    m_lineEdit = new KLineEdit(hbox);
    m_lineEdit->setText("kdelibs5");
    m_lineEdit->setClearButtonShown(true);
    connect(m_lineEdit, SIGNAL(returnPressed()), this, SLOT(updateLabels()));

    QPushButton *pushButton = new QPushButton(hbox);
    pushButton->setIcon(KIcon("system-software-update"));
    pushButton->setText(i18n("Update Listing"));
    connect(pushButton, SIGNAL(clicked()), this, SLOT(updateLabels()));

    KVBox *vbox = new KVBox(mainWidget);
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
    setCentralWidget(mainWidget);

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
    // Catch changes that could happen while we're open by reloading the cache.
    m_backend->reloadCache();
    // You'd usually call this after searching for updates. This example can't
    // do that yet, so just call it here for the sake of example-ing.

    m_package = m_backend->package(m_lineEdit->text());

    m_nameLabel->setText(i18n("<b>Package:</b> %1", m_package->name()));
    m_stateLabel->setText(i18n("<b>State:</b> %1", m_package->state()));
    m_sectionLabel->setText(i18n("<b>Section:</b> %1", m_package->section()));
    QString installedSize(KGlobal::locale()->formatByteSize(m_package->installedSize()));
    m_installedSizeLabel->setText(i18n("<b>Installed Size:</b> %1", installedSize));
    m_maintainerLabel->setText(i18n("<b>Maintainer:</b> %1", m_package->maintainer()));
    m_sourceLabel->setText(i18n("<b>Source package:</b> %1", m_package->sourcePackage()));
    m_sourceLabel->setText(i18n("<b>Version:</b> %1", m_package->version()));
    QString packageSize(KGlobal::locale()->formatByteSize(m_package->availablePackageSize()));
    m_packageSizeLabel->setText(i18n("<b>Size:</b> %1", packageSize));
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

#include "qapttest.moc"
