#pragma once

#include <QtCore/QString>

#include <nx/utils/uuid.h>

#include "upload_state.h"

namespace nx {
namespace client {
namespace desktop {

struct WearableState
{
    enum Status
    {
        Unlocked,
        LockedByOtherClient,
        Locked,
        Uploading,
        Consuming,
    };

    struct EnqueuedFile
    {
        QString path;
        qint64 startTimeMs = 0;
    };

    /** Current wearable status of the camera. */
    Status status = Unlocked;

    /** Id of the camera. */
    QnUuid cameraId;

    /** Id of the user that has locked this camera. */
    QnUuid lockUserId;

    EnqueuedFile currentFile;

    UploadState currentUpload;

    int consumeProgress = 0;

    /** Upload queue. */
    QList<EnqueuedFile> queue;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::WearableState)
