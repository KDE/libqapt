/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Michael D. Stemle <themanchicken@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SOURCESLISTTEST_H
#define SOURCESLISTTEST_H

#include <QObject>
#include <QStringList>

#include "sourceslist.h"
#include "sourceentry.h"

class SourcesListTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testConstructor();
    void testLoadSourcesOneFile();
    void testLoadSourcesManyFiles();
    void testAddSource();
    void testRemoveSource();
    void testSaveSources();

private:
    QStringList sampleSourcesHasOneFile;
    QStringList sampleSourcesHasTwoFiles;
    QStringList sampleSourcesHasDuplicateFiles;
    QString outputFile;
    QString dummyFile;
    
    void verifySourceEntry (const QString           &label,
                            const QApt::SourceEntry &entry,
                            const QString           &type,
                            const QString           &uri,
                            const QString           &dist,
                            const QString           &components,
                            const QString           &archs,
                            const bool              isEnabled,
                            const bool              isValid
                           );
};

#endif // SOURCESLISTTEST_H
