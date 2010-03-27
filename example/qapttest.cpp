/*
 * qapttest.cpp
 *
 * Copyright (C) 2008 Jonathan Thomas <echidnaman@kubuntu.org>
 */
#include "qapttest.h"

#include <QtGui/QLabel>

#include <KVBox>
#include <kconfigdialog.h>
#include <kstatusbar.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kstandardaction.h>
#include <KDE/KLocale>
#include <KIcon>

#include <../src/backend.h>
#include <../src/package.h>

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

    QApt::Backend *backend = new QApt::Backend();
    backend->init();

    QApt::Package *package = backend->package("kdelibs5");

    KVBox *mainWidget = new KVBox(this);

    QLabel* nameLabel = new QLabel(i18n("<b>Package:</b> %1", package->name()), mainWidget);
    QLabel* sectionLabel = new QLabel(i18n("<b>Section:</b> %1", package->section()), mainWidget);
    QLabel* installedSizeLabel = new QLabel(i18n("<b>Installed Size:</b> %1", package->installedSize()), mainWidget);
    QLabel* maintainerLabel = new QLabel(i18n("<b>Maintainer:</b> %1", package->maintainer()), mainWidget);
    QLabel* sourceLabel = new QLabel(i18n("<b>Source package:</b> %1", package->sourcePackage()), mainWidget);
    QLabel* packageSizeLabel = new QLabel(i18n("<b>Size:</b> %1", package->availablePackageSize()), mainWidget);
    QLabel* shortDescriptionLabel = new QLabel(i18n("<b>Description:</b> %1", package->shortDescription()), mainWidget);
    QLabel* longDescriptionLabel = new QLabel(package->longDescription(), mainWidget);

    setCentralWidget(mainWidget);
}

qapttest::~qapttest()
{
}

#include "qapttest.moc"
