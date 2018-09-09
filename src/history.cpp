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

#include "history.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSharedData>
#include <QStringBuilder>

// APT includes
#include <apt-pkg/configuration.h>

namespace QApt {

class HistoryItemPrivate : public QSharedData
{
    public:
        HistoryItemPrivate(const QString &data)
            : QSharedData()
            , isValid(true)
        {
            parseData(data);
        }

         HistoryItemPrivate(const HistoryItemPrivate &other)
            : QSharedData(other)
            , startDate(other.startDate)
            , installedPackages(other.installedPackages)
            , upgradedPackages(other.upgradedPackages)
            , downgradedPackages(other.downgradedPackages)
            , removedPackages(other.removedPackages)
            , purgedPackages(other.purgedPackages)
            , error(other.error)
            , isValid(other.isValid)
        {
        }

        // Data
        QDateTime startDate;
        QStringList installedPackages;
        QStringList upgradedPackages;
        QStringList downgradedPackages;
        QStringList removedPackages;
        QStringList purgedPackages;
        QString error;
        bool isValid;

        void parseData(const QString &data);
};

void HistoryItemPrivate::parseData(const QString &data)
{
    QStringList lines = data.split(QLatin1Char('\n'));

    int lineIndex = 0;
    bool dateFound = false;
    bool errorFound = false;

    QStringList actionStrings;
    actionStrings << QLatin1String("Install") << QLatin1String("Upgrade")
                  << QLatin1String("Downgrade") << QLatin1String("Remove")
                  << QLatin1String("Purge");


    while (lineIndex < lines.size()) {
        QString line = lines.at(lineIndex);
        // skip empty lines and lines beginning with '#'
        if (line.isEmpty() || line.at(0) == '#') {
            lineIndex++;
            continue;
        }

        QStringList keyValue = line.split(QLatin1String(": "));
        int actionIndex = actionStrings.indexOf(keyValue.value(0));
        Package::State action;

        // Invalid
        if (keyValue.size() < 2) {
            isValid = false;
            lineIndex++;
            continue;
        }

        if (!dateFound && (keyValue.value(0) == QLatin1String("Start-Date"))) {
            startDate = QDateTime::fromString(keyValue.value(1), QLatin1String("yyyy-MM-dd  hh:mm:ss"));
        } else if (actionIndex > -1) {
            switch(actionIndex) {
            case 0:
                action = Package::ToInstall;
                break;
            case 1:
                action = Package::ToUpgrade;
                break;
            case 2:
                action = Package::ToDowngrade;
                break;
            case 3:
                action = Package::ToRemove;
                break;
            case 4:
                action = Package::ToPurge;
                break;
            default:
                break;
            }

            QString actionPackages = keyValue.value(1);
            // Remove arch info
            actionPackages.remove(QRegExp(QLatin1String(":\\w+")));

            for (QString package : actionPackages.split(QLatin1String("), "))) {
                if (!package.endsWith(QLatin1Char(')'))) {
                    package.append(QLatin1Char(')'));
                }

                switch (action) {
                case Package::ToInstall:
                    installedPackages << package;
                    break;
                case Package::ToUpgrade:
                    upgradedPackages << package;
                    break;
                case Package::ToDowngrade:
                    downgradedPackages << package;
                    break;
                case Package::ToRemove:
                    removedPackages << package;
                    break;
                case Package::ToPurge:
                    purgedPackages << package;
                    break;
                default:
                    break;
                }
            }
        } else if (!errorFound && (keyValue.value(0) == QLatin1String("Error"))) {
            error = keyValue.value(1);
        }

        lineIndex++;
    }
}

HistoryItem::HistoryItem(const QString &data)
       : d(new HistoryItemPrivate(data))
{
}

HistoryItem::HistoryItem(const HistoryItem &other)
{
    d = other.d;
}

HistoryItem::~HistoryItem()
{
}

QDateTime HistoryItem::startDate() const
{
    return d->startDate;
}

QStringList HistoryItem::installedPackages() const
{
    return d->installedPackages;
}

QStringList HistoryItem::upgradedPackages() const
{
    return d->upgradedPackages;
}

QStringList HistoryItem::downgradedPackages() const
{
    return d->downgradedPackages;
}

QStringList HistoryItem::removedPackages() const
{
    return d->removedPackages;
}

QStringList HistoryItem::purgedPackages() const
{
    return d->purgedPackages;
}

QString HistoryItem::errorString() const
{
    return d->error;
}

bool HistoryItem::isValid() const
{
    return d->isValid;
}



class HistoryPrivate
{
    public:
        HistoryPrivate(const QString &fileName) : historyFilePath(fileName)
        {
            init();
        }

        // Data
        QString historyFilePath;
        HistoryItemList historyItemList;

        void init();
};

void HistoryPrivate::init()
{
    QString data;

    QFileInfo historyFile(historyFilePath);
    QString directoryPath = historyFile.absoluteDir().absolutePath();
    QDir logDirectory(directoryPath);
    QStringList logFiles = logDirectory.entryList(QDir::Files, QDir::Name);

    QString fullPath;

    for (const QString &file : logFiles) {
        fullPath = directoryPath % '/' % file;
        if (fullPath.contains(QLatin1String("history"))) {
            if (fullPath.endsWith(QLatin1String(".gz"))) {
                QProcess gunzip;
                gunzip.start(QLatin1String("gunzip"), QStringList() << QLatin1String("-c") << fullPath);
                gunzip.waitForFinished();

                data.append(gunzip.readAll());
            } else {
                QFile historyFile(fullPath);

                if (historyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    data.append(historyFile.readAll());
                }
            }
        }
    }

    data = data.trimmed();
    QStringList stanzas = data.split(QLatin1String("\n\n"));
    for (const QString &stanza : stanzas) {
        const HistoryItem historyItem(stanza);
        if (historyItem.isValid())
            historyItemList << historyItem;
    }
}

History::History(QObject *parent)
        : QObject(parent)
        , d_ptr(new HistoryPrivate(QString::fromStdString(_config->FindFile("Dir::Log::History"))))
{
}

History::~History()
{
    delete d_ptr;
}

HistoryItemList History::historyItems() const
{
    Q_D(const History);

    return d->historyItemList;
}

void History::reload()
{
    Q_D(History);

    d->historyItemList.clear();
    d->init();
}
 
}
