#pragma once

struct QnLiveStreamParams
{
    static const float FPS_NOT_INITIALIZED;
    static const int MIN_SECOND_STREAM_FPS = 2;

    Qn::StreamQuality quality = Qn::QualityNotDefined;
    Qn::SecondStreamQuality secondaryQuality = Qn::SSQualityLow;
    float fps = 0;
    int bitrateKbps = 0;

    QnLiveStreamParams();
    bool operator ==(const QnLiveStreamParams& rhs);
    bool operator !=(const QnLiveStreamParams& rhs);
};

