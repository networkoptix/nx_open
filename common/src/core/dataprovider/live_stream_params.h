#pragma once

struct QnLiveStreamParams
{
    static const float kFpsNotInitialized;
    static const int kMinSecondStreamFps = 2;

    Qn::StreamQuality quality = Qn::QualityNotDefined;
    Qn::SecondStreamQuality secondaryQuality = Qn::SSQualityLow;
    float fps = 0;
    int bitrateKbps = 0;

    QnLiveStreamParams();
    bool operator ==(const QnLiveStreamParams& rhs) const;
    bool operator !=(const QnLiveStreamParams& rhs) const;
};

