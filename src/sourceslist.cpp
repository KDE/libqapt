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
#include <QDebug>

// APT includes
#include <apt-pkg/configuration.h>

// Own includes
#include "dbusinterfaces_p.h"

namespace QApt {

class SourcesListPrivate
{
public:
    SourcesListPrivate(const QStringList &sourcesList = QStringList())
        : sourceFiles(QStringList())
        , list(QHash<QString,QApt::SourceEntryList>())
    {
        if ( sourcesList.isEmpty() ) {
            setDefaultSourcesFiles();
        } else {
            addSourcesFileList(sourcesList);
        }
        reload();
    }

    // File list
    QStringList sourceFiles;

    // DBus
    WorkerInterface *worker;

    // Data
    QHash< QString, QApt::SourceEntryList > list;

    void setDefaultSourcesFiles();
    void reload();
    void load(const QString &filePath);

    void addSourcesFile(const QString &filePath);
    void addSourcesFileList(const QStringList &filePathList);
};

void SourcesListPrivate::addSourcesFile(const QString& filePath)
{
    // Just some basic deduping
    if (sourceFiles.contains(filePath)) {
        return;
    }

    sourceFiles.append(filePath);
}

void SourcesListPrivate::addSourcesFileList(const QStringList& filePathList)
{
    for (const QString &filePathEntry : filePathList) {
        addSourcesFile(filePathEntry);
    }
}

void SourcesListPrivate::setDefaultSourcesFiles()
{
    addSourcesFile(QString::fromStdString(_config->FindFile("Dir::Etc::sourcelist")));

    // Go through the parts dir and append those
    QDir partsDir(QString::fromStdString(_config->FindFile("Dir::Etc::sourceparts")));
    for (const QString& file : partsDir.entryList(QStringList() << "*.list")) {
        addSourcesFile(partsDir.filePath(file));
    }

    return;
}

void SourcesListPrivate::reload()
{
    list.clear();

    for (const QString &file : sourceFiles) {
        if (!file.isNull() && !file.isEmpty() ) {
            load(file);
        }
    }
}

void SourcesListPrivate::load(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QFile::Text | QIODevice::ReadOnly)) {
        qWarning() << "Unable to open the file " << filePath << " as read-only text: " << file.errorString();
        return;
    }

    // Make a source entry for each line in the file
    while (!file.atEnd()) {
        QString line = file.readLine();
        list[filePath].append(SourceEntry(line, filePath));
    }
}

SourcesList::SourcesList(QObject *parent)
    : QObject(parent)
    , d_ptr(new SourcesListPrivate())
{
    Q_D(SourcesList);

    d->worker = new WorkerInterface(QLatin1String(s_workerReverseDomainName),
                                                  QLatin1String("/"), QDBusConnection::systemBus(),
                                                  this);
}

SourcesList::SourcesList(QObject *parent, const QStringList &sourcesFileList)
    : QObject(parent)
    , d_ptr(new SourcesListPrivate(sourcesFileList))
{
    Q_D(SourcesList);

    d->worker = new WorkerInterface(QLatin1String(s_workerReverseDomainName),
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

    SourceEntryList to_return;

    for (const SourceEntryList &oneList : d->list.values()) {
        to_return.append(oneList);
    }

    return to_return;
}

SourceEntryList SourcesList::entries(const QString &sourceFile) const
{
    Q_D(const SourcesList);

    return d->list[sourceFile];
}

void SourcesList::reload()
{
    Q_D(SourcesList);

    d->reload();
}

void SourcesList::addEntry(const SourceEntry &entry)
{
    Q_D(SourcesList);

    QString entryForFile = entry.file();

    // Default to the zeroth file if no file is specified.
    if (entryForFile.isEmpty()) {
        entryForFile = sourceFiles().at(0);
    }

    // TODO: More sophisticated dupe checking, e.g. in the case of adding new components
    for (const SourceEntry &item : entries(entryForFile)) {
        if (entry == item) {
            return;
        }
    }

    d->list[entryForFile].append(entry);
}

void SourcesList::removeEntry(const SourceEntry &entry)
{
    Q_D(SourcesList);

    // If we have a file in the entry given, optimize the remove.
    const QString &entryForFile = entry.file();
    if (!entryForFile.isEmpty()) {
        d->list[entryForFile].removeAll(entry);
        return;
    }

    for (QString &sourcesFile : sourceFiles()) {
        d->list[sourcesFile].removeAll(entry);
    }

    return;
}

bool SourcesList::containsEntry(const SourceEntry& entry, const QString &sourceFile)
{
    // Don't optimize for the file if we don't have a usable file
    if (sourceFile.isEmpty()) {
        return entries().contains(entry);
    }

    return entries(sourceFile).contains(entry);
}

QStringList SourcesList::sourceFiles()
{
    Q_D(SourcesList);

    return d->sourceFiles;
}

QString SourcesList::dataForSourceFile(const QString& sourceFile)
{
    QString to_return;

    for (const SourceEntry &listEntry : entries(sourceFile)) {
        to_return.append(listEntry.toString() + '\n');
    }

    if (to_return.isEmpty()) {
        to_return.append(QString("## See sources.list(5) for more information, especially\n"
                                 "# Remember that you can only use http, ftp or file URIs.\n"
                                 "# CDROMs are managed through the apt-cdrom tool.\n"));
    }

    return to_return;
}

QString SourcesList::toString() const
{
    Q_D(const SourcesList);

    QString toReturn;

    for (const QString &sourceFile : d->sourceFiles) {
        toReturn += sourceFile + '\n';
        for (const QApt::SourceEntry &sourceEntry : entries(sourceFile)) {
            toReturn += sourceEntry.toString() + '\n';
        }
    }

    return toReturn;
}

void SourcesList::save()
{
    Q_D(SourcesList);

    for (const QString &sourceFile : sourceFiles()) {
        qDebug() << "Writing file " << sourceFile << " with: " << dataForSourceFile(sourceFile);
        if (! d->worker->writeFileToDisk(dataForSourceFile(sourceFile), sourceFile)) {
            qWarning() << "Failed to write the file to disk (dbus call failed)!";
        }
    }
}

}
