#include "live_stream_params.h"

const float QnLiveStreamParams::kFpsNotInitialized = -1.0;

QnLiveStreamParams::QnLiveStreamParams():
    quality(Qn::QualityNormal),
    secondaryQuality(Qn::SSQualityNotDefined),
    fps(kFpsNotInitialized)
{
}

bool QnLiveStreamParams::operator==(const QnLiveStreamParams& rhs) const
{
    return rhs.quality == quality
        && rhs.secondaryQuality == secondaryQuality
        && rhs.fps == fps
        && rhs.bitrateKbps == bitrateKbps;
}

bool QnLiveStreamParams::operator !=(const QnLiveStreamParams& rhs) const
{
    return !(*this == rhs);
}
