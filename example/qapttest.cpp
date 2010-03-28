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
    : KXmlGuiWindow()
{
    // add a status bar
    statusBar()->show();

    // a call to KXmlGuiWindow::setupGUI() populates the GUI
    // with actions, using KXMLGUI.
    // It also applies the saved mainwindow settings, if any, and ask the
    // mainwindow to automatically save settings if changed: window size,
    // toolbar position, icon size, etc.
    setupGUI();
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
    m_sectionLabel = new QLabel(vbox);
    m_installedSizeLabel = new QLabel(vbox);
    m_maintainerLabel = new QLabel(vbox);
    m_sourceLabel = new QLabel(vbox);
    m_packageSizeLabel = new QLabel(vbox);
    m_shortDescriptionLabel = new QLabel(vbox);
    m_longDescriptionLabel = new QLabel(vbox);

    updateLabels();
    setCentralWidget(mainWidget);
}

qapttest::~qapttest()
{
}

void qapttest::updateLabels()
{
    m_package = m_backend->package(m_lineEdit->text());

    m_nameLabel->setText(i18n("<b>Package:</b> %1", m_package->name()));
    m_sectionLabel->setText(i18n("<b>Section:</b> %1", m_package->section()));
    QString installedSize(KGlobal::locale()->formatByteSize(m_package->installedSize()));
    m_installedSizeLabel->setText(i18n("<b>Installed Size:</b> %1", installedSize));
    m_maintainerLabel->setText(i18n("<b>Maintainer:</b> %1", m_package->maintainer()));
    m_sourceLabel->setText(i18n("<b>Source package:</b> %1", m_package->sourcePackage()));
    QString packageSize(KGlobal::locale()->formatByteSize(m_package->availablePackageSize()));
    m_packageSizeLabel->setText(i18n("<b>Size:</b> %1", packageSize));
    m_shortDescriptionLabel->setText(i18n("<b>Description:</b> %1", m_package->shortDescription()));
    m_longDescriptionLabel->setText(m_package->longDescription());
}

#include "qapttest.moc"
