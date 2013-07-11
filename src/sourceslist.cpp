/***************************************************************************
 *   Copyright © 2013 Jonathan Thomas <echidnaman@kubuntu.org>             *
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

#include "sourceslist.h"

// Qt includes
#include <QDir>

// APT includes
#include <apt-pkg/configuration.h>

// Own includes
#include "workerdbus.h" // OrgKubuntuQaptworker2Interface

namespace QApt {

class SourcesListPrivate
{
public:
    SourcesListPrivate()
    {
        reload();
    }

    // DBus
    OrgKubuntuQaptworker2Interface *worker;

    // Data
    QString filePath;
    SourceEntryList list;

    void reload();
    void load(const QString &filePath);
};

void SourcesListPrivate::reload()
{
    filePath = QString::fromStdString(_config->FindFile("Dir::Etc::sourcelist"));
    QDir partsDir(QString::fromStdString(_config->FindFile("Dir::Etc::sourceparts")));

    // Load sources.list plus sources.list.d/ files
    load(filePath);

    for (const QString& file : partsDir.entryList(QStringList() << "*.list")) {
        load(partsDir.filePath(file));
    }
}

void SourcesListPrivate::load(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::Text | QIODevice::ReadOnly))
        return;

    // Make a source entry for each line in the file
    while (!file.atEnd()) {
        QString line = file.readLine();
        list.append(SourceEntry(line, filePath));
    }
}

SourcesList::SourcesList(QObject *parent)
    : QObject(parent)
    , d_ptr(new SourcesListPrivate())
{
    Q_D(SourcesList);

    d->worker = new OrgKubuntuQaptworker2Interface(QLatin1String("org.kubuntu.qaptworker2"),
                                                  QLatin1String("/"), QDBusConnection::systemBus(),
                                                  this);
}

SourcesList::~SourcesList()
{
    delete d_ptr;
}

SourceEntryList SourcesList::entries() const
{
    Q_D(const SourcesList);

    return d->list;
}

void SourcesList::reload()
{
    Q_D(SourcesList);

    d->list.clear();
    d->reload();
}

void SourcesList::addEntry(const SourceEntry &entry)
{
    Q_D(SourcesList);

    // TODO: More sophisticated dupe checking, e.g. in the case of adding new components
    for (const SourceEntry &item : this->entries()) {
        if (entry == item)
            return;
    }

    d->list.append(entry);
}

void SourcesList::removeEntry(const SourceEntry &entry)
{
    Q_D(SourcesList);

    d->list.removeAll(entry);
}

void SourcesList::save()
{
    Q_D(SourcesList);

    // Write an empty default file if list is empty
    if (d->list.isEmpty()) {
        QString path = QString::fromStdString(_config->FindFile("Dir::Etc::sourcelist"));
        QString header = QString("## See sources.list(5) for more information, especially\n"
                                 "# Remember that you can only use http, ftp or file URIs.\n"
                                 "# CDROMs are managed through the apt-cdrom tool.\n");

        d->worker->writeFileToDisk(header, path);
        return;
    }

    // Otherwise, go through our list of source entries and write them to their
    // respective files
    QHash<QString, QString> files;
    for (SourceEntry &entry : d->list) {
        QString file = files[entry.file()];

        // Compose file
        QString data = entry.toString() + '\n';
        file.append(data);
        files[entry.file()] = file;
    }

    // Write all files
    auto iter = files.constBegin();
    while (iter != files.constEnd()) {
        QString data = iter.value();
        QString filePath = iter.key();
        d->worker->writeFileToDisk(data, filePath);
        ++iter;
    }
}

}
