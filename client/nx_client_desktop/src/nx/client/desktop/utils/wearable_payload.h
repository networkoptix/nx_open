#pragma once

#include <QtCore/QVector>
#include <QtCore/QString>

#include <api/model/wearable_check_data.h>
#include <api/model/wearable_check_reply.h>

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
        ChunksTakenByFileInQueue,
        ChunksTakenOnServer,
        NoSpaceOnServer,
        ServerError
    };

    QString path;
    Status status = Valid;
    QnWearableCheckDataElement local;
    QnWearableCheckReplyElement remote;

    static bool allHaveStatus(const WearablePayloadList& list, Status status)
    {
        if (list.empty())
            return false;

        for (const WearablePayload& payload : list)
            if (payload.status != status)
                return false;

        return true;
    }

    static bool someHaveStatus(const WearablePayloadList& list, Status status)
    {
        if (list.empty())
            return false;

        for (const WearablePayload& payload : list)
            if (payload.status == status)
                return true;

        return false;
    }
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::WearablePayloadList)
