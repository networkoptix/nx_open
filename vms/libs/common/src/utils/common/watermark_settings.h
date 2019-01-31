#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct QnWatermarkSettings
{
    int useWatermark = false;

    double frequency = 0.5; //< 0..1
    double opacity = 0.3; //< 0..1
};

#define QnWatermarkSettings_Fields (useWatermark)(frequency)(opacity)

QN_FUSION_DECLARE_FUNCTIONS(QnWatermarkSettings, (json)(eq))

Q_DECLARE_METATYPE(QnWatermarkSettings)
