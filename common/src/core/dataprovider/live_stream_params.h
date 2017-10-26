#pragma once

struct QnLiveStreamParams
{
    static const float FPS_NOT_INITIALIZED;

    Qn::StreamQuality quality = Qn::QualityNotDefined;
    Qn::SecondStreamQuality secondaryQuality = Qn::SSQualityLow;
    float fps = 0;
    int bitrateKbps = 0;

    QnLiveStreamParams();
    bool operator ==(const QnLiveStreamParams& rhs);
    bool operator !=(const QnLiveStreamParams& rhs);
};

