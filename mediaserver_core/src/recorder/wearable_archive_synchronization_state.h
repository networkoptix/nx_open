#pragma once

#include <QtCore/QString>

namespace nx {
namespace mediaserver_core {
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

        return 100 * processed / duration;
    }
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx

Q_DECLARE_METATYPE(nx::mediaserver_core::recorder::WearableArchiveSynchronizationState);
