#pragma once

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct UploadState
{
    enum Status
    {
        Initial,
        CalculatingMD5,
        CreatingUpload,
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

    // Error message, if any.
    QString errorMessage;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::UploadState)
