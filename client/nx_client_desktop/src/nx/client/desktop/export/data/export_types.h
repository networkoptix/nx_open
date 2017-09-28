#pragma once

#include <QtCore/QMetaType>

enum class StreamRecorderError;

namespace nx {
namespace client {
namespace desktop {

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
};

ExportProcessError convertError(StreamRecorderError value);

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::ExportProcessStatus)
