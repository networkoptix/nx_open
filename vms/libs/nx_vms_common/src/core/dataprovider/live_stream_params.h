// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

#include <nx/fusion/model_functions_fwd.h>
#include <common/common_globals.h>

struct NX_VMS_COMMON_API QnLiveStreamParams
{
    static const float kFpsNotInitialized;

    Qn::StreamQuality quality = Qn::StreamQuality::undefined;
    float fps = kFpsNotInitialized;
    float bitrateKbps = 0;
    QSize resolution;
    QString codec;

    bool operator==(const QnLiveStreamParams& other) const = default;

    QString toString() const;
};
#define QnLiveStreamParams_Fields (quality)(fps)(bitrateKbps)(resolution)(codec)
QN_FUSION_DECLARE_FUNCTIONS(QnLiveStreamParams, (json), NX_VMS_COMMON_API)

struct QnAdvancedStreamParams
{
    QnLiveStreamParams primaryStream;
    QnLiveStreamParams secondaryStream;
};
#define QnAdvancedStreamParams_Fields (primaryStream)(secondaryStream)
QN_FUSION_DECLARE_FUNCTIONS(QnAdvancedStreamParams, (json), NX_VMS_COMMON_API)
