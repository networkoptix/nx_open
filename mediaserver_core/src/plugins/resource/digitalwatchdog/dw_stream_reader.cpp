#ifdef ENABLE_ONVIF

#include "dw_stream_reader.h"
#include "digital_watchdog_resource.h"

CameraDiagnostics::Result QnDwStreamReader::openStreamInternal(
    bool isCameraControlRequired,
    const QnLiveStreamParams& params)
{
    const auto streamIndex = getRole() == Qn::CR_SecondaryLiveVideo
        ? Qn::StreamIndex::secondary : Qn::StreamIndex::primary;
    if (isCameraControlRequired)
        getResource().dynamicCast<QnDigitalWatchdogResource>()->setVideoCodec(streamIndex, params.codec.toLower());
    return base_type::openStreamInternal(isCameraControlRequired, params);
}

#endif //ENABLE_ONVIF
