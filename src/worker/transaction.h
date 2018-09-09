/***************************************************************************
 *   Copyright © 2012 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *   Copyright © 2008-2009 Sebastian Heinlein <devel@glatzor.de>           *
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

#ifndef TRANSACTION_H
#define TRANSACTION_H

// Qt includes
#include <QMutex>
#include <QObject>
#include <QDBusContext>
#include <QDBusVariant>

// Own includes
#include "downloadprogress.h"

class QTimer;
class TransactionQueue;

class Transaction : public QObject, protected QDBusContext
{
    Q_OBJECT

    Q_CLASSINFO("D-Bus Interface", "org.kubuntu.qaptworker.transaction")
    Q_PROPERTY(QString transactionId READ transactionId CONSTANT)
    Q_PROPERTY(int userId READ userId CONSTANT)
    Q_PROPERTY(int role READ role)
    Q_PROPERTY(int status READ status)
    Q_PROPERTY(int error READ error)
    Q_PROPERTY(QString locale READ locale)
    Q_PROPERTY(QString proxy READ proxy)
    Q_PROPERTY(QString debconfPipe READ debconfPipe)
    Q_PROPERTY(QVariantMap packages READ packages)
    Q_PROPERTY(bool isCancellable READ isCancellable)
    Q_PROPERTY(bool isCancelled READ isCancelled)
    Q_PROPERTY(int exitStatus READ exitStatus)
    Q_PROPERTY(bool isPaused READ isPaused)
    Q_PROPERTY(QString statusDetails READ statusDetails)
    Q_PROPERTY(int progress READ progress)
    Q_PROPERTY(QApt::DownloadProgress downloadProgress READ downloadProgress)
    Q_PROPERTY(QStringList untrustedPackages READ untrustedPackages)
    Q_PROPERTY(quint64 downloadSpeed READ downloadSpeed)
    Q_PROPERTY(quint64 downloadETA READ downloadETA)
    Q_PROPERTY(QString filePath READ filePath)
    Q_PROPERTY(QString errorDetails READ errorDetails)
    Q_PROPERTY(int frontendCaps READ frontendCaps)
public:
    Transaction(TransactionQueue *queue, int userId);
    Transaction(TransactionQueue *queue, int userId,
                QApt::TransactionRole role, QVariantMap packagesList);
    ~Transaction();

    QString transactionId() const;
    int userId() const;
    int role();
    int status();
    int error();
    QString locale();
    QString proxy();
    QString debconfPipe();
    QVariantMap packages();
    bool isCancellable();
    bool isCancelled();
    int exitStatus();
    QString medium();
    bool isPaused();
    QString statusDetails();
    int progress();
    QString service() const;
    QApt::DownloadProgress downloadProgress();
    QStringList untrustedPackages();
    bool allowUntrusted();
    quint64 downloadSpeed();
    quint64 downloadETA();
    QString filePath();
    QString errorDetails();
    bool safeUpgrade() const;
    bool replaceConfFile() const;
    int frontendCaps() const;

    void setStatus(QApt::TransactionStatus status);
    void setError(QApt::ErrorCode code);
    void setCancellable(bool isCancellable);
    void setExitStatus(QApt::ExitStatus exitStatus);
    void setMediumRequired(const QString &label, const QString &medium);
    void setIsPaused(bool paused);
    void setStatusDetails(const QString &details);
    void setProgress(int progress);
    void setService(const QString &service);
    void setDownloadProgress(const QApt::DownloadProgress &downloadProgress);
    void setUntrustedPackages(const QStringList &untrusted, bool promptUser);
    void setDownloadSpeed(quint64 downloadSpeed);
    void setETA(quint64 ETA);
    void setFilePath(const QString &filePath);
    void setErrorDetails(const QString &errorDetails);
    void setSafeUpgrade(bool safeUpgrade);
    void setConfFileConflict(const QString &currentPath, const QString &newPath);
    void setFrontendCaps(int frontendCaps);

private:
    // Pointers to external containers
    TransactionQueue *m_queue;

    // Transaction data
    QString m_tid;
    int m_uid;
    QApt::TransactionRole m_role;
    QApt::TransactionStatus m_status;
    QApt::ErrorCode m_error;
    QString m_locale;
    QString m_proxy;
    QString m_debconfPipe;
    QVariantMap m_packages;
    bool m_isCancellable;
    bool m_isCancelled;
    QApt::ExitStatus m_exitStatus;
    QString m_medium;
    bool m_isPaused;
    QString m_statusDetails;
    int m_progress;
    QApt::DownloadProgress m_downloadProgress;
    QStringList m_untrusted;
    bool m_allowUntrusted;
    quint64 m_downloadSpeed;
    quint64 m_ETA;
    QString m_filePath;
    QString m_errorDetails;
    bool m_safeUpgrade;
    QString m_currentConfPath;
    bool m_replaceConfFile;
    QApt::FrontendCaps m_frontendCaps;

    // Other data
    QMap<int, QString> m_roleActionMap;
    QTimer *m_idleTimer;
    QMutex m_dataMutex;
    QString m_service;

    // Private functions
    int dbusSenderUid() const;
    bool isForeignUser() const;
    void setRole(int role);
    void setLocale(QString locale);
    void setProxy(QString proxy);
    void setDebconfPipe(QString pipe);
    void setPackages(QVariantMap packageList);
    bool authorizeRun();

Q_SIGNALS:
    Q_SCRIPTABLE void propertyChanged(int role, QDBusVariant newValue);
    Q_SCRIPTABLE void finished(int exitStatus);
    Q_SCRIPTABLE void mediumRequired(QString label, QString mountPoint);
    Q_SCRIPTABLE void promptUntrusted(QStringList untrustedPackages);
    Q_SCRIPTABLE void configFileConflict(QString currentPath, QString newPath);
    void idleTimeout(Transaction *trans);
    
public Q_SLOTS:
    void setProperty(int property, QDBusVariant value);
    void run();
    void cancel();
    void provideMedium(const QString &medium);
    void replyUntrustedPrompt(bool approved);
    void resolveConfigFileConflict(const QString &currentPath, bool replaceFile);

private Q_SLOTS:
    void emitIdleTimeout();
};

#endif // TRANSACTION_H
