/***************************************************************************
 *   Copyright © 2014 Harald Sitter <sitter@kde.org>                       *
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

#include "sourceslisttest.h"

#include <QtTest>

#include <dependencyinfo.h>

namespace QApt {

class DependencyInfoTest : public QObject
{
    Q_OBJECT
private slots:
    void testSimpleParse();
    void testOrDependency();

    void testMultiArchAnnotation();
    void testNoMultiArchAnnotation();
};

void DependencyInfoTest::testSimpleParse()
{
    // Build list of [ dep1 dep2 dep3 ] to later use for comparison.
    int depCount = 3;
    QStringList packageNames;
    for (int i = 0; i < depCount; ++i) {
        packageNames.append(QStringLiteral("dep%1").arg(QString::number(i)));
    }

    QString field = packageNames.join(",");
    DependencyType depType = Depends;
    auto depList = DependencyInfo::parseDepends(field, depType);
    QCOMPARE(depList.size(), depCount);

    for (int i = 0; i < depCount; ++i) {
        auto depGroup = depList.at(i);
        QCOMPARE(depGroup.size(), 1);
        QCOMPARE(depGroup.first().packageName(), packageNames.at(i));
    }
}

void DependencyInfoTest::testOrDependency()
{
    QString field = QStringLiteral("dep1 | dep2");
    DependencyType depType = Depends;
    auto depList = DependencyInfo::parseDepends(field, depType);
    QCOMPARE(depList.size(), 1);
    auto depGroup = depList.first();
    QCOMPARE(depGroup.size(), 2); // Must have two dependency in this group.
    QCOMPARE(depGroup.at(0).packageName(), QStringLiteral("dep1"));
    QCOMPARE(depGroup.at(1).packageName(), QStringLiteral("dep2"));
}

void DependencyInfoTest::testMultiArchAnnotation()
{
    QString field = QStringLiteral("dep1:any");
    DependencyType depType = Depends;
    auto depList = DependencyInfo::parseDepends(field, depType);
    for (auto depGroup : depList) {
        for (auto dep : depGroup) {
            QCOMPARE(dep.packageName(), field.split(':').first());
            QCOMPARE(dep.multiArchAnnotation(), QStringLiteral("any"));
        }
    }
}

void DependencyInfoTest::testNoMultiArchAnnotation()
{
    QString field = QStringLiteral("dep1");
    DependencyType depType = Depends;
    auto depList = DependencyInfo::parseDepends(field, depType);
    for (auto depGroup : depList) {
        for (auto dep : depGroup) {
            QCOMPARE(dep.packageName(), field);
            QVERIFY(dep.multiArchAnnotation().isEmpty());
        }
    }
}

}

QTEST_MAIN(QApt::DependencyInfoTest);

#include "dependencyinfotest.moc"
