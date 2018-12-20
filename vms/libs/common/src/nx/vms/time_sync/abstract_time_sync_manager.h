#pragma once

#include <chrono>

#include <QtCore/QObject>

namespace nx {
namespace vms {
namespace time_sync {

class AbstractTimeSyncManager: public QObject
{
    Q_OBJECT
public:
    virtual ~AbstractTimeSyncManager() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual std::chrono::milliseconds getSyncTime(
        bool* outIsTimeTakenFromInternet = nullptr) const = 0;
signals:
    /** Emitted when synchronized time has been changed. */
    void timeChanged(qint64 syncTimeMs);
};

} // namespace time_sync
} // namespace vms
} // namespace nx
