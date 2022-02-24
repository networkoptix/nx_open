// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

namespace nx::vms::common {

class NX_VMS_COMMON_API AbstractTimeSyncManager: public QObject
{
    Q_OBJECT
public:
    virtual ~AbstractTimeSyncManager() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual std::chrono::milliseconds getSyncTime(
        bool* outIsTimeTakenFromInternet = nullptr) const = 0;

    /** Forced re-syncronization. */
    virtual void resync() = 0;

signals:
    /** Emitted when synchronized time has been changed. */
    void timeChanged(qint64 syncTimeMs);
};

using AbstractTimeSyncManagerPtr = std::shared_ptr<nx::vms::common::AbstractTimeSyncManager>;

} // namespace nx::vms::common
