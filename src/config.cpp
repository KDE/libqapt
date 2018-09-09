/***************************************************************************
 *   Copyright © 2010-2012 Jonathan Thomas <echidnaman@kubuntu.org>        *
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

#include "config.h" // krazy:exclude=includes

// Qt includes
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QLatin1String>
#include <QDBusConnection>

// APT includes
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>

// Own includes
#include "dbusinterfaces_p.h"

namespace QApt {

const QString APT_CONFIG_PATH = QString("/etc/apt/apt.conf");

class ConfigPrivate
{
    public:
        // DBus
        WorkerInterface *worker;

        // Data
        QByteArray buffer;
        bool newFile;

        void writeBufferEntry(const QByteArray &key, const QByteArray &value);
};

void ConfigPrivate::writeBufferEntry(const QByteArray &key, const QByteArray &value)
{
    int lineIndex = 0;
    bool changed = false;

    QList<QByteArray> lines = buffer.split('\n');

    // We will re-add final newline on write
    if (lines.last().isEmpty()) {
        lines.removeLast();
    }

    // Try to replace key if it's there
    while (lineIndex < lines.size()) {
        QByteArray line = lines[lineIndex];
        // skip empty lines and lines beginning with '#'
        if (line.isEmpty() || line.at(0) == '#') {
            lineIndex++;
            continue;
        }

        QByteArray aKey;
        int eqpos = line.indexOf(' ');

        if (eqpos < 0) {
            // Invalid
            lineIndex++;
            continue;
        } else {
            aKey = line.left(eqpos);

            if (aKey == key) {
                lines[lineIndex] = QByteArray(key + ' ' + value);
                changed = true;
            }
        }

        lineIndex++;
    }

    QByteArray tempBuffer;

    if (changed) {
        // No new keys. Recompose lines and set buffer to new buffer
        Q_FOREACH (const QByteArray &line, lines) {
            tempBuffer += QByteArray(line + '\n');
        }

        buffer = tempBuffer;
    } else {
        // New key or new file. Append to existing buffer
        tempBuffer = QByteArray(key + ' ' + value);
        tempBuffer.append('\n');

        buffer += tempBuffer;
    }
}

Config::Config(QObject *parent)
       : QObject(parent)
       , d_ptr(new ConfigPrivate)
{
    Q_D(Config);

    d->worker = new WorkerInterface(QLatin1String(s_workerReverseDomainName),
                                    QLatin1String("/"), QDBusConnection::systemBus(),
                                    this);

    QFile file(APT_CONFIG_PATH);

    if (file.exists()) {
        file.open(QIODevice::ReadOnly | QIODevice::Text);

        d->buffer = file.readAll();
        d->newFile = false;
    } else {
        d->newFile = true;
    }
}

Config::~Config()
{
    delete d_ptr;
}

bool Config::readEntry(const QString &key, const bool defaultValue) const
{
    return _config->FindB(key.toStdString(), defaultValue);
}

int Config::readEntry(const QString &key, const int defaultValue) const
{
    return _config->FindI(key.toStdString(), defaultValue);
}

QString Config::readEntry(const QString &key, const QString &defaultValue) const
{
    return QString::fromStdString(_config->Find(key.toStdString(), defaultValue.toStdString()));
}

QString Config::findDirectory(const QString &key, const QString &defaultValue) const
{
    return QString::fromStdString(_config->FindDir(key.toStdString().data(), defaultValue.toStdString().data()));
}

QString Config::findFile(const QString& key, const QString& defaultValue) const
{
    return QString::fromStdString(_config->FindFile(key.toLocal8Bit().data(), defaultValue.toLocal8Bit().data()));
}

QStringList Config::architectures() const
{
    QStringList archList;
    std::vector<std::string> archs = APT::Configuration::getArchitectures(false);

    for (std::string &arch : archs)
    {
         archList.append(QString::fromStdString(arch));
    }

    return archList;
}

void Config::writeEntry(const QString &key, const bool value)
{
    Q_D(Config);

    QByteArray boolString;

    boolString = value ? "\"true\";" : "\"false\";";

    if (d->newFile) {
        d->buffer.append(key + ' ' + boolString);
        d->newFile = false;
    } else {
        d->writeBufferEntry(key.toLatin1(), boolString);
    }

    _config->Set(key.toLatin1(), value);
    d->worker->writeFileToDisk(QString(d->buffer), APT_CONFIG_PATH);
}

void Config::writeEntry(const QString &key, const int value)
{
    Q_D(Config);

    QByteArray intString;

    intString = '\"' + QString::number(value).toLatin1() + "\";";

    if (d->newFile) {
        d->buffer.append(key.toLatin1() + ' ' + intString);
        d->newFile = false;
    } else {
        d->writeBufferEntry(key.toLatin1(), intString);
    }

    _config->Set(key.toLatin1(), value);
    d->worker->writeFileToDisk(QString(d->buffer), APT_CONFIG_PATH);
}

void Config::writeEntry(const QString &key, const QString &value)
{
    Q_D(Config);

    QByteArray valueString;

    valueString = '\"' + value.toLatin1() + "\";";

    if (d->newFile) {
        d->buffer.append(key + ' ' + valueString);
        d->newFile = false;
    } else {
        d->writeBufferEntry(key.toLatin1(), valueString);
    }

    _config->Set(key.toStdString(), value.toStdString());
    d->worker->writeFileToDisk(QString(d->buffer), APT_CONFIG_PATH);
}

}
