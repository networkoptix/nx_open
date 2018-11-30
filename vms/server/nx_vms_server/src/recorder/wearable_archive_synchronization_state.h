#pragma once

#include <QtCore/QString>

namespace nx {
namespace vms::server {
namespace recorder {

struct WearableArchiveSynchronizationState {
    enum Status
    {
        Empty,
        Consuming,
        Finished,
    };

    Status status = Empty;
    qint64 duration = 0;
    qint64 processed = 0;
    QString errorString;

    bool isConsuming() const
    {
        return status == Consuming;
    }

    int progress() const
    {
        if (duration == 0)
            return 0;

        if (status == Finished)
            return 100;

        qint64 result = 100 * processed / duration;
        return result <= 99 ? result : 99;
    }
};

} // namespace recorder
} // namespace vms::server
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::server::recorder::WearableArchiveSynchronizationState);
