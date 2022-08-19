// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <recording/stream_recorder_data.h>

#include <QtCore/QMetaType>

#include <optional>

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
    internalError,
    unsupportedMedia,
    unsupportedFormat,
    ffmpegError,
    incompatibleCodec,
    fileAccess,
    dataNotFound,
    transcodingRequired,
    encryptedArchive,
    temporaryUnavailable,
};

ExportProcessError convertError(const std::optional<nx::recording::Error>& value);

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::ExportProcessStatus)
