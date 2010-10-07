/***************************************************************************
 *   Copyright © 2010 Harald Sitter <apachelogger@ubuntu.com>              *
 *                                                                         *
 *   Inspired by akdebug.cpp (part of Akonadi)                             *
 *   Copyright © 2008 Volker Krause <vkrause@kde.org>                      *
 *                                                                         *
 *       Inspired by kdelibs/kdecore/io/kdebug.h                           *
 *       Copyright © 1997 Matthias Kalle Dalheimer <kalle@kde.org>         *
 *       Copyright © 2002 Holger Freyther <freyther@kde.org>               *
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

#include "debug.h"

#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMutex>

#warning currently the debugger does not have a hard limit for overwrite, nor does it rotate (rotate should be distro dependent though)

class FileDebugStream : public QIODevice
{
public:
    FileDebugStream() :
            m_type(QtDebugMsg)
    {
        open(WriteOnly);
    }

    qint64 readData(char * data, qint64 maxSize) {
        Q_UNUSED(data);
        Q_UNUSED(maxSize);
        return 0;
    }

    qint64 writeData(const char *data, qint64 maxSize)
    {
        QByteArray buffer = QByteArray::fromRawData(data, maxSize);

        if (!m_fileName.isEmpty()) {
            QFile outputFile(m_fileName);
            outputFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered);
            outputFile.write(QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss.zzz ")).toAscii());
            outputFile.write(data, maxSize);
            outputFile.putChar('\n');
            outputFile.close();
        }

        qt_message_output(m_type, buffer.trimmed().constData());
        return maxSize;
    }

    void setFileName(const QString &fileName)
    {
        m_fileName = fileName;
    }

    void setType(const QtMsgType &type)
    {
        m_type = type;
    }

private:
    QtMsgType m_type;
    QString m_fileName;
};

class DebugPrivate
{
public:
    DebugPrivate() :
            m_fileStream(new FileDebugStream())
    {
        m_fileStream->setFileName(logFileName());
    }

    ~DebugPrivate()
    {
        delete m_fileStream;
    }

    QString logFileName() const
    {
        #warning TODO: errorLogFileName
        return QString(QLatin1String("/var/log/qaptworker.log"));
    }

    QDebug stream(const QtMsgType &type)
    {
        QMutexLocker locker(&m_mutex);
        m_fileStream->setType(type);
        return QDebug(m_fileStream);
    }

    QMutex m_mutex;
    FileDebugStream *m_fileStream;
};

Q_GLOBAL_STATIC(DebugPrivate, s_instance)

QDebug aptDebug()
{
    return s_instance()->stream(QtDebugMsg);
}
