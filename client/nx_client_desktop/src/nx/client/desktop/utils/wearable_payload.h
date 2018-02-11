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
};

struct WearableUpload
{
    WearablePayloadList elements;
    qint64 spaceAvailable = 0;
    qint64 spaceRequested = 0;

    bool allHaveStatus(WearablePayload::Status status) const
    {
        if (elements.empty())
            return false;

        for (const WearablePayload& payload : elements)
            if (payload.status != status)
                return false;

        return true;
    }

    bool someHaveStatus(WearablePayload::Status status) const
    {
        if (elements.empty())
            return false;

        for (const WearablePayload& payload : elements)
            if (payload.status == status)
                return true;

        return false;
    }

};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::WearableUpload)
