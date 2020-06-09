#ifdef ENABLE_ONVIF

#include "vivotek_stream_reader.h"

namespace nx::vms::server::plugins {

VivotekStreamReader::VivotekStreamReader(const QnSharedResourcePointer<VivotekResource>& resource): 
    base_type(resource)
{
}

CameraDiagnostics::Result VivotekStreamReader::fetchUpdateVideoEncoder(
    CameraInfoParams* outInfo, 
    bool isPrimary, 
    bool isCameraControlRequired,
    const QnLiveStreamParams& params) const
{
    auto result = 
        base_type::fetchUpdateVideoEncoder(outInfo, isPrimary, isCameraControlRequired, params);
    if (!result)
        return result;

    const auto vivotekResource = getResource().dynamicCast<VivotekResource>();
    if (params.codec != "H265" || !vivotekResource || !isCameraControlRequired)
        return CameraDiagnostics::NoErrorResult();

    // Setup fps and bitrate for H.265 codec via native API.

    if (!qFuzzyIsNull(params.bitrateKbps))
    {
        const auto result = vivotekResource->setVivotekParameter(
            "h265_maxvbrbitrate", QString::number((int) (params.bitrateKbps * 1000)), isPrimary);
        if (!result)
            return result;
    }

    return vivotekResource->setVivotekParameter(
        "h265_maxframe", QString::number((int) params.fps), isPrimary);
}

} // namespace nx::vms::server::plugins

#endif //ENABLE_ONVIF
