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
    if (params.codec != "H265" || !vivotekResource)
        return CameraDiagnostics::NoErrorResult();

    // Setup fps for H.265 codec via native API.
    return vivotekResource->setVivotekParameter(
        "h265_maxframe", QString::number((int) params.fps), isPrimary);
}

} // namespace nx::vms::server::plugins

#endif //ENABLE_ONVIF
