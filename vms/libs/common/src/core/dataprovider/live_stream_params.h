#pragma once

#include <QtCore/QSize>

#include <nx/fusion/model_functions_fwd.h>
#include <common/common_globals.h>

struct QnLiveStreamParams
{
    static const float kFpsNotInitialized;
    static const int kMinSecondStreamSharedFps = 2;

    Qn::StreamQuality quality = Qn::StreamQuality::undefined;
    float fps = kFpsNotInitialized;
    float bitrateKbps = 0;
    QSize resolution;
    QString codec;

    QString toString() const;
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
