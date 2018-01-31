#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <common/common_globals.h>

struct QnLiveStreamParams
{
    static const float kFpsNotInitialized;
    static const int kMinSecondStreamFps = 2;

    Qn::StreamQuality quality = Qn::QualityNotDefined;
    float fps = kFpsNotInitialized;
    int bitrateKbps = 0;
    QSize resolution;
    QString codec;
};
#define QnLiveStreamParams_Fields (quality)(fps)(bitrateKbps)(resolution)(codec)
QN_FUSION_DECLARE_FUNCTIONS(QnLiveStreamParams, (eq)(json))

struct QnAdvancedStreamParams
{
    QnLiveStreamParams primaryStream;
    QnLiveStreamParams secondaryStream;
};
#define QnAdvancedStreamParams_Fields (primaryStream)(secondaryStream)
QN_FUSION_DECLARE_FUNCTIONS(QnAdvancedStreamParams, (json))