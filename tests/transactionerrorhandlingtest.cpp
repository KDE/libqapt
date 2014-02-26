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

#include "transactionerrorhandlingtest.h"

#include <QtTest>
#include <transaction.h>

QTEST_MAIN(TransactionErrorHandlingTest);

#define DUMMY_DETAILS                       "Dummy Details"

#define EXPECTED_MESSAGE_UNKNOWN_ERROR      "An unknown error has occurred, here are the details: Dummy Details"

#define EXPECTED_MESSAGE_INIT_ERROR         "The package system could not be initialized, your configuration may be broken."

#define EXPECTED_MESSAGE_LOCK_ERROR         "Another application seems to be using the package system at this time. You must close all other package managers before you will be able to install or remove any packages."

#define EXPECTED_MESSAGE_DISKSPACE_ERROR    "You do not have enough disk space in the directory at Dummy Details to continue with this operation."

#define EXPECTED_MESSAGE_FETCH_ERROR        "Could not download packages"

#define EXPECTED_MESSAGE_COMMIT_ERROR       "An error occurred while applying changes: Dummy Details"

#define EXPECTED_MESSAGE_AUTH_ERROR         "This operation cannot continue since proper authorization was not provided"

#define EXPECTED_MESSAGE_WORKER_DISAPPEARED_ERROR "It appears that the QApt worker has either crashed or disappeared. Please report a bug to the QApt maintainers"

#define EXPECTED_MESSAGE_UNTRUSTED_ERROR    "The following package(s) has not been verified by its author(s). Downloading untrusted packages has been disallowed by your current configuration."

// The following two message are for once we finally have the plural forms working in QObject::tr()... but that seems like a lower priority.
#define EXPECTED_MESSAGE_UNTRUSTED_ERROR_1  "The following package has not been verified by its author. Downloading untrusted packages has been disallowed by your current configuration."

#define EXPECTED_MESSAGE_UNTRUSTED_ERROR_2  "The following packages have not been verified by their authors. Downloading untrusted packages has been disallowed by your current configuration."

#define EXPECTED_MESSAGE_DOWNLOAD_DISALLOWED_ERROR "Downloads are currently disallowed."

#define EXPECTED_MESSAGE_NOTFOUND_ERROR     "Error: Not Found"

#define EXPECTED_MESSAGE_WRONGARCH_ERROR    "Error: Wrong architecture"

#define EXPECTED_MESSAGE_MARKING_ERROR      "Error: Marking error"

void TransactionErrorHandlingTest::initTestCase()
{
    // Called before the first testfunction is executed
    QWARN( "PLEASE NOTE: You're going to see some dbus warnings, that's expected. Carry on." );
    this->subject = new QApt::Transaction( 0 );
}

void TransactionErrorHandlingTest::cleanupTestCase()
{
    // Called after the last testfunction was executed
    delete this->subject;
}

void TransactionErrorHandlingTest::init()
{
    // Called before each testfunction is executed
}

void TransactionErrorHandlingTest::cleanup()
{
    // Called after every testfunction
}

void TransactionErrorHandlingTest::testSuccess()
{    
    this->subject->updateError(QApt::ErrorCode::Success);
    
    QVERIFY2(
        this->subject->error() == QApt::ErrorCode::Success,
        "Verify we have a successful error code..."
    );
    
    QVERIFY2(
        this->subject->errorString().isNull(),
        "Verify that we got an empty string back after setting the error code to zero..."
    );
}

void TransactionErrorHandlingTest::testUnknownError()
{
    QString dummyDetails( DUMMY_DETAILS );
    this->subject->updateError( QApt::ErrorCode::UnknownError );
    this->subject->updateErrorDetails( dummyDetails );
    
    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_UNKNOWN_ERROR,
        "Verify that we got the error message we expected to get for UnknownError..."
    );
    
    this->subject->updateErrorDetails( QString() );
}

void TransactionErrorHandlingTest::testInitError()
{
    this->subject->updateError(QApt::ErrorCode::InitError);

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_INIT_ERROR,
        "Verify that we got the error message we expected to get for InitError..."
    );
}

void TransactionErrorHandlingTest::testLockError()
{
    this->subject->updateError( QApt::ErrorCode::LockError );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_LOCK_ERROR,
        "Verify that we got the error message we expected to get for LockError..."
    );
}

void TransactionErrorHandlingTest::testDiskSpaceError()
{
    QString dummyDetails( DUMMY_DETAILS );
    this->subject->updateError( QApt::ErrorCode::DiskSpaceError );
    this->subject->updateErrorDetails( dummyDetails );
    
    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_DISKSPACE_ERROR,
        "Verify that we got the error message we expected to get for DiskSpaceError..."
    );
    
    this->subject->updateErrorDetails( QString() );
}

void TransactionErrorHandlingTest::testFetchError()
{
    this->subject->updateError( QApt::ErrorCode::FetchError );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_FETCH_ERROR,
        "Verify that we got the error message we expected to get for FetchError..."
    );
}

void TransactionErrorHandlingTest::testCommitError()
{
    QString dummyDetails( DUMMY_DETAILS );
    this->subject->updateError( QApt::ErrorCode::CommitError );
    this->subject->updateErrorDetails( dummyDetails );
    
    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_COMMIT_ERROR,
        "Verify that we got the error message we expected to get for CommitError..."
    );
    
    this->subject->updateErrorDetails( QString() );
}

void TransactionErrorHandlingTest::testAuthError()
{
    this->subject->updateError( QApt::ErrorCode::AuthError );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_AUTH_ERROR,
        "Verify that we got the error message we expected to get for AuthError..."
    );
}

void TransactionErrorHandlingTest::testWorkerDisappeared()
{
    this->subject->updateError( QApt::ErrorCode::WorkerDisappeared );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_WORKER_DISAPPEARED_ERROR,
        "Verify that we got the error message we expected to get for WorkerDisappeared..."
    );
}

void TransactionErrorHandlingTest::testUntrustedError()
{
    QStringList untrusted;
    
    untrusted << "one";
    
    this->subject->updateError( QApt::ErrorCode::UntrustedError );
    this->subject->updateUntrustedPackages( (const QStringList&) untrusted );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_UNTRUSTED_ERROR,
        "Verify that we got the error message we expected to get for UntrustedError..."
    );
}

void TransactionErrorHandlingTest::testDownloadDisallowedError()
{
    this->subject->updateError( QApt::ErrorCode::DownloadDisallowedError );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_DOWNLOAD_DISALLOWED_ERROR,
        "Verify that we got the error message we expected to get for DownloadDisallowedError..."
    );
}

void TransactionErrorHandlingTest::testNotFoundError()
{
    this->subject->updateError( QApt::ErrorCode::NotFoundError );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_NOTFOUND_ERROR,
        "Verify that we got the error message we expected to get for NotFoundError..."
    );
}

void TransactionErrorHandlingTest::testWrongArchError()
{
    this->subject->updateError( QApt::ErrorCode::WrongArchError );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_WRONGARCH_ERROR,
        "Verify that we got the error message we expected to get for WrongArchError..."
    );
}

void TransactionErrorHandlingTest::testMarkingError()
{
    this->subject->updateError( QApt::ErrorCode::MarkingError );

    QVERIFY2(
        this->subject->errorString() == EXPECTED_MESSAGE_MARKING_ERROR,
        "Verify that we got the error message we expected to get for MarkingError..."
    );
}

// #include "transactionerrorhandlingtest.moc"
