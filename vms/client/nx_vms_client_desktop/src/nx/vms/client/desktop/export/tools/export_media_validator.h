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
        nonContinuosAvi,
        downscaling,
        tooLong,
        tooBigExeFile,
        transcodingInLayoutIsNotSupported,
        nonCameraResources,

        count
    };
    using Results = std::bitset<int(Result::count)>;

    static Results validateSettings(const ExportMediaSettings& settings);
    static Results validateSettings(const ExportLayoutSettings& settings);

    /** Check if exe file will be greater than 4 Gb. */
    static bool exeFileIsTooBig(const QnLayoutResourcePtr& layout, qint64 durationMs);
    /** Check if exe file will be greater than 4 Gb. */
    static bool exeFileIsTooBig(const QnMediaResourcePtr& mediaResource, qint64 durationMs);
};

} // namespace nx::vms::client::desktop
