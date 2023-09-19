// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "synctime.h"

#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/time/abstract_time_sync_manager.h>
#include <nx_ec/managers/abstract_time_manager.h>

struct QnSyncTime::Private
{
    ec2::AbstractTimeNotificationManagerPtr timeNotificationManager;
    nx::vms::common::AbstractTimeSyncManagerPtr timeSyncManager;
    mutable nx::Mutex mutex;
};

static QnSyncTime* s_instance = nullptr;

QnSyncTime::QnSyncTime(QObject *parent):
    QObject(parent),
    d(new Private())
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;
}

QnSyncTime::~QnSyncTime()
{
    // It should help tracking a crash when application exits.
    NX_VERBOSE(this, "~QnSyncTime()");
    if (s_instance == this)
        s_instance = nullptr;
}

QnSyncTime* QnSyncTime::instance()
{
    return s_instance;
}

QDateTime QnSyncTime::currentDateTime() const
{
    return QDateTime::fromMSecsSinceEpoch(currentMSecsSinceEpoch());
}

qint64 QnSyncTime::currentUSecsSinceEpoch() const
{
    return currentMSecsSinceEpoch() * 1000;
}

std::chrono::milliseconds QnSyncTime::value() const
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    return d->timeSyncManager
        ? d->timeSyncManager->getSyncTime()
        : std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch());
}

std::chrono::microseconds QnSyncTime::currentTimePoint() const
{
    return value();
}

qint64 QnSyncTime::currentMSecsSinceEpoch() const
{
    return value().count();
}

void QnSyncTime::resync()
{
    if (d->timeSyncManager)
        d->timeSyncManager->resync();
}

void QnSyncTime::setTimeSyncManager(nx::vms::common::AbstractTimeSyncManagerPtr timeSyncManager)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    if (d->timeSyncManager)
        d->timeSyncManager->disconnect(this);

    d->timeSyncManager = timeSyncManager;

    if (d->timeSyncManager)
    {
        connect(
            d->timeSyncManager.get(),
            &nx::vms::common::AbstractTimeSyncManager::timeChanged,
            this,
            &QnSyncTime::timeChanged);
    }
}

void QnSyncTime::setTimeNotificationManager(
    ec2::AbstractTimeNotificationManagerPtr timeNotificationManager)
{
    NX_MUTEX_LOCKER lock(&d->mutex);

    if (d->timeNotificationManager)
        d->timeNotificationManager->disconnect(this);

    d->timeNotificationManager = timeNotificationManager;
    if (d->timeNotificationManager)
    {
        connect(d->timeNotificationManager.get(),
            &ec2::AbstractTimeNotificationManager::primaryTimeServerTimeChanged,
            this,
            [this]()
            {
                if (d->timeSyncManager)
                    d->timeSyncManager->resync();
            });
    }
}
