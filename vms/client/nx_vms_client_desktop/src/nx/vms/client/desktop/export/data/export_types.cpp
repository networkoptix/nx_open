#include "export_types.h"

#include <recording/stream_recorder_data.h>

namespace nx::vms::client::desktop {

ExportProcessError convertError(StreamRecorderError value)
{
    switch (value)
    {
        case StreamRecorderError::containerNotFound:
            return ExportProcessError::unsupportedFormat;

        case StreamRecorderError::videoStreamAllocation:
        case StreamRecorderError::audioStreamAllocation:
            return ExportProcessError::ffmpegError;

        case StreamRecorderError::incompatibleCodec:
            return ExportProcessError::incompatibleCodec;

        case StreamRecorderError::invalidAudioCodec:
        case StreamRecorderError::invalidResourceType:
            return ExportProcessError::unsupportedMedia;

        case StreamRecorderError::fileCreate:
        case StreamRecorderError::fileWrite:
            return ExportProcessError::fileAccess;

        case StreamRecorderError::dataNotFound:
            return ExportProcessError::dataNotFound;

        default:
            break;
    }

    return ExportProcessError::noError;
}

} // namespace nx::vms::client::desktop
