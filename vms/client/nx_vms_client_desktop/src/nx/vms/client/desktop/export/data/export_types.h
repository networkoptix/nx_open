#pragma once

#include <QtCore/QMetaType>

enum class StreamRecorderError;

namespace nx::vms::client::desktop {

struct ExportMediaSettings;
struct ExportLayoutSettings;
class AbstractExportTool;

enum class ExportProcessStatus
{
    initial,
    exporting,
    success,
    failure,
    cancelling,
    cancelled
};

enum class ExportProcessError
{
    noError,
    unsupportedMedia,
    unsupportedFormat,
    ffmpegError,
    incompatibleCodec,
    fileAccess,
    dataNotFound
};

ExportProcessError convertError(StreamRecorderError value);

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportProcessStatus)
