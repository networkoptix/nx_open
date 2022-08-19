// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_types.h"

#include <recording/stream_recorder_data.h>

namespace nx::vms::client::desktop {

ExportProcessError convertError(const std::optional<nx::recording::Error>& value)
{
    using namespace nx::recording;
    if (!value)
        return ExportProcessError::noError;

    switch (value->code)
    {
        case Error::Code::unknown:
        case Error::Code::resourceNotFound: //< Exists in cloud part only.
            return ExportProcessError::internalError;

        case Error::Code::containerNotFound:
            return ExportProcessError::unsupportedFormat;

        case Error::Code::videoStreamAllocation:
        case Error::Code::audioStreamAllocation:
        case Error::Code::metadataStreamAllocation:
            return ExportProcessError::ffmpegError;

        case Error::Code::incompatibleCodec:
            return ExportProcessError::incompatibleCodec;

        case Error::Code::invalidAudioCodec:
        case Error::Code::invalidResourceType:
            return ExportProcessError::unsupportedMedia;

        case Error::Code::fileCreate:
        case Error::Code::fileWrite:
            return ExportProcessError::fileAccess;

        case Error::Code::dataNotFound:
            return ExportProcessError::dataNotFound;

        case Error::Code::transcodingRequired:
            return ExportProcessError::transcodingRequired;

        case Error::Code::encryptedArchive:
            return ExportProcessError::encryptedArchive;

        case Error::Code::temporaryUnavailable:
            return ExportProcessError::temporaryUnavailable;
    }

    return ExportProcessError::internalError;
}

} // namespace nx::vms::client::desktop
