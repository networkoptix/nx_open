// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>

#include <QtCore/QDateTime>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace ec2 {

class AbstractTimeNotificationManager;
using AbstractTimeNotificationManagerPtr = std::shared_ptr<AbstractTimeNotificationManager>;

} // namespace ec2

namespace nx::vms::common {

class AbstractTimeSyncManager;
using AbstractTimeSyncManagerPtr = std::shared_ptr<AbstractTimeSyncManager>;

} // namespace nx::vms::common

/**
 * Time provider that is synchronized with Server.
 */
class NX_VMS_COMMON_API QnSyncTime final: public QObject
{
    Q_OBJECT;

public:
    QnSyncTime(QObject *parent = NULL);
    ~QnSyncTime();

    static QnSyncTime* instance();

    void setTimeSyncManager(nx::vms::common::AbstractTimeSyncManagerPtr timeSyncManager);
    void setTimeNotificationManager(ec2::AbstractTimeNotificationManagerPtr timeNotificationManager);

    /**
     * Synchronized time of the System in milliseconds since epoch. Can be taken from the Internet
     * if there is a network access, otherwise one of the Servers is chosen as main, and it's time
     * is used.
     * If System connection is not established yet, local OS time is used.
     */
    std::chrono::milliseconds value() const;

    qint64 currentMSecsSinceEpoch() const;
    qint64 currentUSecsSinceEpoch() const;
    std::chrono::microseconds currentTimePoint() const;
    QDateTime currentDateTime() const;

    /** Forced re-synchronization. */
    void resync();

signals:
    /**
     * Emitted whenever time on Server changes. Deprecated. Use AbstractTimeSyncManager instead.
     */
    void timeChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

#define qnSyncTime (QnSyncTime::instance())
