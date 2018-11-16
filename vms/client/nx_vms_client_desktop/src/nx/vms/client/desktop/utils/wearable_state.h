#pragma once

#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <recording/time_period.h>

#include "upload_state.h"

class QnTimePeriodList;

namespace nx::vms::client::desktop {

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

    enum Error {
        NoError,
        LockRequestFailed,
        CameraSnatched,
        UploadFailed,
        ConsumeRequestFailed,
    };

    struct EnqueuedFile
    {
        QString path;
        qint64 startTimeMs;
        QnTimePeriod uploadPeriod;
    };

    /** Current wearable status of the camera. */
    Status status = Unlocked;

    /** Error, if any. */
    Error error = NoError;

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
     * @returns                         Whether this state represents a cancellable operation.
     */
    bool isCancellable() const;

    /**
     * @returns                         Progress the whole operation, a number in [0, 100] range.
     */
    int progress() const;

    /**
     * @returns                         A union of all time periods for the files in this state.
     */
    QnTimePeriodList periods() const;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::WearableState)
