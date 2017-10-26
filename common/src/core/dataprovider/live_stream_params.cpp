#include "live_stream_params.h"

const float QnLiveStreamParams::FPS_NOT_INITIALIZED = -1.0;

QnLiveStreamParams::QnLiveStreamParams():
    quality(Qn::QualityNormal),
    secondaryQuality(Qn::SSQualityNotDefined),
    fps(FPS_NOT_INITIALIZED)
{
}

bool QnLiveStreamParams::operator==(const QnLiveStreamParams& rhs)
{
    return rhs.quality == quality
        && rhs.secondaryQuality == secondaryQuality
        && rhs.fps == fps
        && rhs.bitrateKbps == bitrateKbps;
}

bool QnLiveStreamParams::operator !=(const QnLiveStreamParams& rhs) { return !(*this == rhs); }
