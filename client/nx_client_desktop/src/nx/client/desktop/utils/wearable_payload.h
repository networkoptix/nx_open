#pragma once

#include <QtCore/QVector>
#include <QtCore/QString>

#include <api/model/wearable_prepare_data.h>
#include <api/model/wearable_prepare_reply.h>

#include "wearable_fwd.h"

namespace nx {
namespace client {
namespace desktop {

struct WearablePayload
{
    enum Status
    {
        Valid,
        FileDoesntExist,
        UnsupportedFormat,
        NoTimestamp,
        FootagePastMaxDays,
        ChunksTakenByFileInQueue,
        ChunksTakenOnServer,
        StorageCleanupNeeded,
        ServerError
    };

    QString path;
    Status status = Valid;
    QnWearablePrepareDataElement local;
    QnWearablePrepareReplyElement remote;
};

struct WearableUpload
{
    WearablePayloadList elements;

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
