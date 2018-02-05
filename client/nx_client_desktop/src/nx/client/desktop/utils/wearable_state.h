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

    /** Upload queue. */
    QVector<EnqueuedFile> queue;

    /** Index of the current file in queue. */
    int currentIndex = 0;

    /** Upload state for the current file. */
    UploadState currentUpload;

    /** Consume progress for the current file. */
    int consumeProgress = 0;

    /**
     * @returns                         File that is currently being processed.
     */
    EnqueuedFile currentFile() const;

    /**
     * @returns                         Whether this state represents an active operation.
     */
    bool isRunning() const;

    /**
     * @returns                         Whether this state represents a finished operation.
     */
    bool isDone() const;

    /**
     * @returns                         Progress the whole operation, a number in [0, 100] range.
     */
    int progress() const;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::WearableState)
