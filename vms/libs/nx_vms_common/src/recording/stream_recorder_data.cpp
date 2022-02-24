// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_recorder_data.h"

#include <nx/utils/log/assert.h>

namespace nx::recording {

Error::Error(Error::Code code, const QnStorageResourcePtr& storage):
    code(code),
    storage(storage)
{
}

QString Error::toString() const
{
    switch (code)
    {
        case Code::unknown:
            NX_ASSERT(false, "Unexpected error code");
            return tr("Internal error");
        case Code::containerNotFound:
            return tr("Corresponding container in FFMPEG library was not found.");
        case Code::fileCreate:
            return tr("Could not create output file for video recording.");
        case Code::videoStreamAllocation:
            return tr("Could not allocate output stream for recording.");
        case Code::audioStreamAllocation:
            return tr("Could not allocate output audio stream.");
        case Code::metadataStreamAllocation:
            return tr("Could not allocate output metadata stream.");
        case Code::invalidAudioCodec:
            return tr("Invalid audio codec information.");
        case Code::incompatibleCodec:
            return tr("Video or audio codec is incompatible with the selected format.");
        case Code::transcodingRequired:
            return tr("Video transcoding required.");
        case Code::fileWrite:
            return tr("File write error. Not enough free space.");
        case Code::invalidResourceType:
            return tr("Invalid resource type for data export.");
        case Code::dataNotFound:
            return tr("No data exported.");
    }

    return QString();
}

} // namespace nx::recording {
