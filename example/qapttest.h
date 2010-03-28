/*
 * qapttest.h
 *
 * Copyright (C) 2008 Jonathan Thomas <echidnaman@kubuntu.org>
 */
#ifndef QAPTTEST_H
#define QAPTTEST_H


#include <KMainWindow>

#include <../src/backend.h>
#include <../src/package.h>

class qapttestView;
class QLabel;
class KToggleAction;
class KLineEdit;

/**
 * This class serves as the main window for qapttest.  It handles the
 * menus, toolbars, and status bars.
 *
 * @short Main window class
 * @author Jonathan Thomas <echidnaman@kubuntu.org>
 * @version 0.1
 */
class qapttest : public KMainWindow
{
    Q_OBJECT
public:
    /**
     * Default Constructor
     */
    qapttest();

    /**
     * Default Destructor
     */
    virtual ~qapttest();

private slots:
    void updateLabels();

private:
    KToggleAction *m_statusbarAction;

    KLineEdit *m_lineEdit;

    QApt::Backend *m_backend;
    QApt::Package *m_package;

    QLabel *m_nameLabel;
    QLabel *m_sectionLabel;
    QLabel *m_installedSizeLabel;
    QLabel *m_maintainerLabel;
    QLabel *m_sourceLabel;
    QLabel *m_packageSizeLabel;
    QLabel *m_shortDescriptionLabel;
    QLabel *m_longDescriptionLabel;
};

#endif
