// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <api/model/virtual_camera_prepare_data.h>
#include <api/model/virtual_camera_prepare_reply.h>

#include "virtual_camera_fwd.h"

namespace nx::vms::client::desktop {

struct VirtualCameraPayload
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
        NoSpaceOnServer,
        ServerError
    };

    QString path;
    Status status = Valid;
    QnVirtualCameraPrepareDataElement local;
    QnVirtualCameraPrepareReplyElement remote;
};

struct VirtualCameraUpload
{
    VirtualCameraPayloadList elements;

    bool allHaveStatus(VirtualCameraPayload::Status status) const
    {
        if (elements.empty())
            return false;

        for (const VirtualCameraPayload& payload: elements)
            if (payload.status != status)
                return false;

        return true;
    }

    bool someHaveStatus(VirtualCameraPayload::Status status) const
    {
        if (elements.empty())
            return false;

        for (const VirtualCameraPayload& payload: elements)
            if (payload.status == status)
                return true;

        return false;
    }

};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::VirtualCameraUpload);
