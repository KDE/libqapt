/*
 * transactionerrorhandlingtest - A test for error handling in QApt::Transaction
 * Copyright 2014  Michael D. Stemle <themanchicken@gmail.com>
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

#ifndef TRANSACTIONERRORHANDLINGTEST_H
#define TRANSACTIONERRORHANDLINGTEST_H

#include <QObject>

#define __CURRENTLY_UNIT_TESTING__ 1

#include <transaction.h>

// namespace QAptTests {
class TransactionErrorHandlingTest : public QObject
{
    Q_OBJECT
    
    QApt::Transaction *subject;
    
private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    void testSuccess();
    void testUnknownError();
    void testInitError();
    void testLockError();
    void testDiskSpaceError();
    void testFetchError();
    void testCommitError();
    void testAuthError();
    void testWorkerDisappeared();
    void testUntrustedError(); // This one should test some plurals in translation
    void testDownloadDisallowedError();
    void testNotFoundError();
    void testWrongArchError();
    void testMarkingError();
};
// }

#endif // TRANSACTIONERRORHANDLINGTEST_H
