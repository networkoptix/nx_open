#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <common/common_globals.h>

struct QnLiveStreamParams
{
    static const float kFpsNotInitialized;
    static const int kMinSecondStreamFps = 2;

    Qn::StreamQuality quality = Qn::QualityNormal;
    float fps = kFpsNotInitialized;
    int bitrateKbps = 0;
};

#define QnLiveStreamParams_Fields (quality)(fps)(bitrateKbps)

//QN_FUSION_DEFINE_FUNCTIONS(QnLiveStreamParams, (eq))
QN_FUSION_DECLARE_FUNCTIONS(QnLiveStreamParams, (eq))
