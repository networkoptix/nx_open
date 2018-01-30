#pragma once

#include <QtCore/QString>

namespace nx {
namespace client {
namespace desktop {

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

    /** Upload id, also serves as the filename on the server. */
    QString id;

    /** Upload size, in bytes. */
    qint64 size = 0;

    /** Total bytes uploaded. */
    qint64 uploaded = 0;

    /** Current status of the upload. */
    Status status = Initial;

    /** Error message, if any. */
    QString errorMessage;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::UploadState)
