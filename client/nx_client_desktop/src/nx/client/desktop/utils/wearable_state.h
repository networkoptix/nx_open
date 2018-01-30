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

    /** File that's currently being processed. */
    EnqueuedFile currentFile;

    /** Upload state for the current file. */
    UploadState currentUpload;

    /** Consume progress for the current file. */
    int consumeProgress = 0;

    /** Upload queue. */
    QList<EnqueuedFile> queue;

    /**
     * @returns                         Whether this state represents an active worker.
     */
    bool isRunning() const;

    /**
     * @returns                         Progress of the current file being processed,
     *                                  a number in [0, 100] range.
     */
    int progress() const;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::WearableState)
