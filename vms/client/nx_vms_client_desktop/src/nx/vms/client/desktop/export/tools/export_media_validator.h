// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <bitset>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

struct ExportMediaSettings;
struct ExportLayoutSettings;

class ExportMediaValidator
{
public:
    enum class Result
    {
        ok,
        transcoding,
        aviWithAudio,
        downscaling,
        tooLong,
        tooBigExeFile,
        transcodingInLayoutIsNotSupported,
        nonCameraResources,
        noCameraData,
        exportNotAllowed,

        count
    };
    using Results = std::bitset<int(Result::count)>;

    static Results validateSettings(const ExportMediaSettings& settings, QnMediaResourcePtr mediaResource);
    static Results validateSettings(const ExportLayoutSettings& settings, QnLayoutResourcePtr layout);

    /** Check if exe file will be greater than 4 Gb. */
    static bool exeFileIsTooBig(const QnLayoutResourcePtr& layout, qint64 durationMs);
    /** Check if exe file will be greater than 4 Gb. */
    static bool exeFileIsTooBig(const QnMediaResourcePtr& mediaResource, qint64 durationMs);
};

} // namespace nx::vms::client::desktop
