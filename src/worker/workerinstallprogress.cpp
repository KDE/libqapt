/***************************************************************************
 *   Copyright © 2010-2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
 *   Copyright © 2004 Canonical <mvo@debian.org>                           *
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

#include "workerinstallprogress.h"

#include <QStringBuilder>
#include <QStringList>
#include <QTextCodec>
#include <QDebug>

#include <apt-pkg/error.h>
#include <apt-pkg/install-progress.h>

#include <errno.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <pty.h>
#include <unistd.h>

#include <iostream>
#include <stdlib.h>

#include "transaction.h"

using namespace std;

WorkerInstallProgress::WorkerInstallProgress(int begin, int end)
        : m_trans(nullptr)
        , m_startCounting(false)
        , m_progressBegin(begin)
        , m_progressEnd(end)
{
    setenv("APT_LISTBUGS_FRONTEND", "none", 1);
    setenv("APT_LISTCHANGES_FRONTEND", "debconf", 1);
}

void WorkerInstallProgress::setTransaction(Transaction *trans)
{
    m_trans = trans;
    std::setlocale(LC_ALL, m_trans->locale().toLatin1());

    // FIXME: bloody workaround.
    //        Since QLocale::system and consequently QTextCodec::forLocale is
    //        set way before we get here there's a need to manually override
    //        the already set default codec with that is requested by the client.
    //        Since the client talks to us about posix locales however we cannot
    //        expect a reliable mapping from split(.).last() to a proper codec.
    //        Also there is no explicit Qt api to translate a given posix locale
    //        to QLocale & QTextCodec.
    //        Ultimately transactions should get new properties for QLocale::name
    //        and QTextCodec::name, assuming generally meaningful values we can
    //        through those properties accurately recreate the client locale env.
    if (m_trans->locale().contains(QChar('.'))) {
        QTextCodec *codec = QTextCodec::codecForName(m_trans->locale().split(QChar('.')).last().toUtf8());
        QTextCodec::setCodecForLocale(codec);
    }

    if ((trans->frontendCaps() & QApt::DebconfCap) && !trans->debconfPipe().isEmpty()) {
        setenv("DEBIAN_FRONTEND", "passthrough", 1);
        setenv("DEBCONF_PIPE", trans->debconfPipe().toLatin1(), 1);
    } else {
        setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    }
}

pkgPackageManager::OrderResult WorkerInstallProgress::start(pkgPackageManager *pm)
{
    m_trans->setStatus(QApt::CommittingStatus);
    pkgPackageManager::OrderResult res;

    res = pm->DoInstallPreFork();
    if (res == pkgPackageManager::Failed) {
        return res;
    }

    int readFromChildFD[2];

    //Initialize both pipes
    if (pipe(readFromChildFD) < 0) {
        return res;
    }

    int pty_master;
    m_child_id = forkpty(&pty_master, 0, 0, 0);

    if (m_child_id == -1) {
        return res;
    } else if (m_child_id == 0) {
        // close pipe we don't need
        close(readFromChildFD[0]);

        APT::Progress::PackageManagerProgressFd progress(readFromChildFD[1]);
        res = pm->DoInstallPostFork(&progress);

        // dump errors into cerr (pass it to the parent process)
        _error->DumpErrors();

        close(readFromChildFD[1]);

        _exit(res);
    }

    // make it nonblocking
    fcntl(readFromChildFD[0], F_SETFL, O_NONBLOCK);
    fcntl(pty_master, F_SETFL, O_NONBLOCK);

    // Update the interface until the child dies
    int ret;
    char masterbuf[1024];
    while (waitpid(m_child_id, &ret, WNOHANG) == 0) {
        // Read dpkg's raw output
        while(read(pty_master, masterbuf, sizeof(masterbuf)) > 0);

        // Update high-level status info
        updateInterface(readFromChildFD[0], pty_master);
    }

    res = (pkgPackageManager::OrderResult)WEXITSTATUS(ret);

    close(readFromChildFD[0]);
    close(readFromChildFD[1]);
    close(pty_master);

    return res;
}

void WorkerInstallProgress::updateInterface(int fd, int writeFd)
{
    char buf[2];
    static char line[1024] = "";

    while (1) {
        int len = read(fd, buf, 1);

        // Status message didn't change
        if (len < 1) {
            break;
        }

        if (buf[0] == '\n') {
            const QStringList list = QString::fromUtf8(line).split(QLatin1Char(':'));
            const QString status = list.at(0);
            const QString package = list.at(1);
            QString percent = list.at(2);
            QString str = list.at(3);
            // If str legitimately had a ':' in it (such as a package version)
            // we need to retrieve the next string in the list.
            if (list.count() == 5) {
                str += QString(':' % list.at(4));
            }

            if (package.isEmpty() || status.isEmpty()) {
                continue;
            }

            if (status.contains(QLatin1String("pmerror"))) {
                // Append error string to existing error details
                m_trans->setErrorDetails(m_trans->errorDetails() % package % '\n' % str % "\n\n");
            } else if (status.contains(QLatin1String("pmconffile"))) {
                // From what I understand, the original file starts after the ' character ('\'') and
                // goes to a second ' character. The new conf file starts at the next ' and goes to
                // the next '.
                QStringList strList = str.split('\'');
                QString oldFile = strList.at(1);
                QString newFile = strList.at(2);

                // Prompt for which file to use if the frontend supports that
                if (m_trans->frontendCaps() & QApt::ConfigPromptCap) {
                    m_trans->setConfFileConflict(oldFile, newFile);
                    m_trans->setStatus(QApt::WaitingConfigFilePromptStatus);

                    while (m_trans->isPaused())
                        usleep(200000);
                }

                m_trans->setStatus(QApt::CommittingStatus);

                if (m_trans->replaceConfFile()) {
                    ssize_t reply = write(writeFd, "Y\n", 2);
                    Q_UNUSED(reply);
                } else {
                    ssize_t reply = write(writeFd, "N\n", 2);
                    Q_UNUSED(reply);
                }
            } else {
                m_startCounting = true;
            }

            int percentage;
            int progress;
            if (percent.contains(QLatin1Char('.'))) {
                QStringList percentList = percent.split(QLatin1Char('.'));
                percentage = percentList.at(0).toInt();
            } else {
                percentage = percent.toInt();
            }

            progress = qRound(qreal(m_progressBegin + qreal(percentage / 100.0) * (m_progressEnd - m_progressBegin)));

            m_trans->setProgress(progress);
            m_trans->setStatusDetails(str);
            // clean-up
            line[0] = 0;
        } else {
            buf[1] = 0;
            strcat(line, buf);
        }
    }
    // 30 frames per second
    usleep(1000000/30);
}
