#pragma once

#include <bitset>

namespace nx {
namespace client {
namespace desktop {

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

};

} // namespace desktop
} // namespace client
} // namespace nx
