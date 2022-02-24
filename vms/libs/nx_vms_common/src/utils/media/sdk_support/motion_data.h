// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::utils::media::sdk_support {

struct MotionData
{
    int channel = 0;
    int durationMs = 0;
    QByteArray data;
};

QN_FUSION_DECLARE_FUNCTIONS(MotionData, (json), NX_VMS_COMMON_API)

} // namespace nx::utils::media::sdk_support