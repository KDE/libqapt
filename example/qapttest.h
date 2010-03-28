/*
 * qapttest.h
 *
 * Copyright (C) 2008 Jonathan Thomas <echidnaman@kubuntu.org>
 */
#ifndef QAPTTEST_H
#define QAPTTEST_H


#include <KMainWindow>

#include <../src/backend.h>

class qapttestView;
class QLabel;
class KToggleAction;
class KLineEdit;

class qapttest : public KMainWindow
{
    Q_OBJECT
public:
    qapttest();

    virtual ~qapttest();

private slots:
    void updateLabels();

private:
    KToggleAction *m_statusbarAction;

    KLineEdit *m_lineEdit;

    QApt::Backend *m_backend;
    QApt::Package *m_package;
    QApt::Group *m_group;

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
