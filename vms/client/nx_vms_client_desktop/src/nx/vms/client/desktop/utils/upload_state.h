// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct UploadState
{
    enum Status
    {
        Initial,
        CalculatingMD5,
        CreatingUpload,
        WaitingFileOnServer,
        Uploading,
        Checking,
        Done,
        Error,
        Canceled
    };

    // Unique upload id.
    // Generated by UploadWorker.
    QString id;

    // Source filename.
    QString source;

    // Upload filename on the server.
    // It will be set to 'source' if empty.
    QString destination;

    // Size of the file.
    qint64 size = 0;

    // Total bytes uploaded.
    qint64 uploaded = 0;

    // UUID of the mediaserver we are uploading this file to.
    QnUuid uuid;

    // Current status of the upload.
    Status status = Initial;

    // TTL for the file, in milliseconds. File will be deleted
    // from the server when TTL passes. -1 means infinite.
    qint64 ttl = -1;

    /** Upload all chunks independently on which chunks are already available on the target. */
    bool uploadAllChunks = false;

    /** Recreate file if it exists. */
    bool recreateFile = false;

    /** Allow uploader to create file on mediaserver. */
    bool allowFileCreation = true;

    // Error message, if any.
    QString errorMessage;
};

} // namespace nx::vms::client::desktop
