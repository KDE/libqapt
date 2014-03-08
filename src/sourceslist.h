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

#ifndef SOURCESLIST_H
#define SOURCESLIST_H

// Qt includes
#include <QObject>

// Own includes
#include "sourceentry.h"

namespace QApt {

class SourcesListPrivate;

class Q_DECL_EXPORT SourcesList : public QObject
{
    Q_OBJECT
public:
    explicit SourcesList(QObject *parent = 0);
    explicit SourcesList(QObject *parent, const QStringList &sourcesFileList);
    ~SourcesList();
    SourceEntryList entries() const;
    SourceEntryList entries(const QString &sourceFile) const;

    void addEntry(const SourceEntry &entry);
    void removeEntry(const SourceEntry &entry);
    bool containsEntry(const SourceEntry &entry, const QString &sourceFile = QString());
    QStringList sourceFiles();
    QString toString() const;
    void save();

private:
    Q_DECLARE_PRIVATE(SourcesList)
    SourcesListPrivate *const d_ptr;

    QString dataForSourceFile(const QString &sourceFile);
    
public Q_SLOTS:
    void reload();
};

}

#endif // SOURCESLIST_H
