/*
 * qapttest.h
 *
 * Copyright (C) 2008 Jonathan Thomas <echidnaman@kubuntu.org>
 */
#ifndef KAPP4_H
#define KAPP4_H


#include <kxmlguiwindow.h>

class qapttestView;
class QPrinter;
class KToggleAction;
class KUrl;

/**
 * This class serves as the main window for qapttest.  It handles the
 * menus, toolbars, and status bars.
 *
 * @short Main window class
 * @author Jonathan Thomas <echidnaman@kubuntu.org>
 * @version 0.1
 */
class qapttest : public KXmlGuiWindow
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

private:
    KToggleAction *m_toolbarAction;
    KToggleAction *m_statusbarAction;
};

#endif // _KAPP4_H_
