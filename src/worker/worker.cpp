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

#include "worker.h"

#include "qaptworkeradaptor.h"

#include <QTimer>
#include <QDebug>

#include <polkit-qt-1/polkitqt1-authority.h>

// Apt includes
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/init.h>

using namespace PolkitQt1;

QAptWorker::QAptWorker(int &argc, char **argv)
        : QCoreApplication(argc, argv)
        , m_progressMeter()
        , m_cache(0)
        , m_policy(0)
        , m_depCache(0)
{
    new QaptworkerAdaptor(this);

    if (!QDBusConnection::systemBus().registerService("org.kubuntu.qaptworker")) {
        qDebug() << QDBusConnection::systemBus().lastError().message();
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        return;
    }

    if (!QDBusConnection::systemBus().registerObject("/", this)) {
        qDebug() << "unable to register service interface to dbus";
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        return;
    }

    initializeApt();

    QTimer::singleShot(3000, this, SLOT(quit()));
}

bool QAptWorker::initializeApt()
{
    m_list = new pkgSourceList;

    if (!pkgInitConfig(*_config)) {
        return false;
    }

    _config->Set("Initialized", 1);

    if (!pkgInitSystem(*_config, _system)) {
        return false;
    }

   // delete any old structures
    if(m_depCache)
        delete m_depCache;
    if(m_policy)
        delete m_policy;
    if(m_cache)
        delete m_cache;

    // Read the sources list
    if (!m_list->ReadMainList()) {
        return false;
    }

    pkgMakeStatusCache(*m_list, m_progressMeter, 0, true);
    qDebug() << "Hi!";
}

QAptWorker::~QAptWorker()
{
}

bool QAptWorker::updateCache()
{
    Authority::Result result;
    SystemBusNameSubject *subject;

    subject = new SystemBusNameSubject(message().service());

    result = Authority::instance()->checkAuthorizationSync("org.kubuntu.qaptworker.updateCache",
             subject , Authority::AllowUserInteraction);
    if (result == Authority::Yes) {
        qDebug() << message().service() << QString("Auth'd");
        // Caller is authorized so we can perform the action
        return true;
    } else {
        qDebug() << message().service() << QString("Can't set the implicit authorization");
        // Caller is not authorized so the action can't be performed
        return false;
    }
}
