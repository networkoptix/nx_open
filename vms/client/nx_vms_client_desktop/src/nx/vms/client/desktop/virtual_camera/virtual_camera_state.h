// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/utils/upload_state.h>
#include <recording/time_period.h>

class QnTimePeriodList;

namespace nx::vms::client::desktop {

struct VirtualCameraState
{
    NX_REFLECTION_ENUM_IN_CLASS(Status,
        Unlocked,
        LockedByOtherClient,
        Locked,
        Uploading,
        Consuming
    );

    NX_REFLECTION_ENUM_IN_CLASS(Error,
        NoError,
        LockRequestFailed,
        CameraSnatched,
        UploadFailed,
        ConsumeRequestFailed
    );

    struct EnqueuedFile
    {
        QString path;
        qint64 startTimeMs;
        QnTimePeriod uploadPeriod;
    };

    /** Current status of the virtual camera. */
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

NX_REFLECTION_INSTRUMENT(VirtualCameraState::EnqueuedFile, (path)(startTimeMs)(uploadPeriod))

// TODO: #sivanov Add `currentUpload` field.
NX_REFLECTION_INSTRUMENT(VirtualCameraState,
    (status)(error)(cameraId)(lockUserId)(queue)(currentIndex)(consumeProgress))

} // namespace nx::vms::client::desktop
