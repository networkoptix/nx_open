// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

struct NX_VMS_COMMON_API QnWatermarkSettings
{
    bool useWatermark = false;

    double frequency = 0.5; //< 0..1
    double opacity = 0.3; //< 0..1

    bool operator==(const QnWatermarkSettings& other) const;
};

#define QnWatermarkSettings_Fields (useWatermark)(frequency)(opacity)
NX_REFLECTION_INSTRUMENT(QnWatermarkSettings, QnWatermarkSettings_Fields)

QN_FUSION_DECLARE_FUNCTIONS(QnWatermarkSettings, (json), NX_VMS_COMMON_API)

Q_DECLARE_METATYPE(QnWatermarkSettings)
