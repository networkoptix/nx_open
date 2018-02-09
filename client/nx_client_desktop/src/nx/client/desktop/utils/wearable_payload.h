#pragma once

#include <QtCore/QVector>
#include <QtCore/QString>

namespace nx {
namespace client {
namespace desktop {

struct WearablePayload;
using WearablePayloadList = QVector<WearablePayload>;

struct WearablePayload
{
    enum Status
    {
        Valid,
        FileDoesntExist,
        UnsupportedFormat,
        NoTimestamp,
        NoSpaceOnServer,
        ChunksTakenOnServer,
    };

    QString path;
    Status status = Valid;
    qint64 size = 0;
    qint64 startTimeMs = 0;
    qint64 durationMs = 0;

    static bool allHaveStatus(const WearablePayloadList& list, Status status)
    {
        if (list.isEmpty())
            return false;

        for (const WearablePayload& payload : list)
            if (payload.status != status)
                return false;
        return true;
    }
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::WearablePayloadList)
