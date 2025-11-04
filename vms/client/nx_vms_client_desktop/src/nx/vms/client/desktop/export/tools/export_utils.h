// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/avi/avi_archive_metadata.h>
#include <nx/media/stream_event.h>
#include <recording/stream_recorder_data.h>

namespace nx::vms::client::desktop {

bool updateSignatureInFile(
    QnStorageResourcePtr storage,
    const QString& fileName,
    QnAviArchiveMetadata::Format fileFormat,
    const QByteArray& signature);

inline nx::recording::Error::Code toRecordingError(nx::media::StreamEvent code)
{
    switch (code)
    {
        case nx::media::StreamEvent::tooManyOpenedConnections:
        case nx::media::StreamEvent::forbiddenWithDefaultPassword:
        case nx::media::StreamEvent::forbiddenWithNoLicense:
        case nx::media::StreamEvent::oldFirmware:
            return nx::recording::Error::Code::temporaryUnavailable;
        case nx::media::StreamEvent::cannotDecryptMedia:
            return nx::recording::Error::Code::encryptedArchive;
        default:
            return nx::recording::Error::Code::unknown;
    }
}

} // namespace nx::vms::client::desktop
