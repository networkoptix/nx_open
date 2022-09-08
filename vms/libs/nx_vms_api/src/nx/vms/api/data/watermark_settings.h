// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/api/data/data_macros.h>

namespace nx::vms::api {

struct NX_VMS_API WatermarkSettings
{
    bool useWatermark = false;

    double frequency = 0.5; //< 0..1
    double opacity = 0.3; //< 0..1

    bool operator==(const WatermarkSettings& other) const;
};

#define WatermarkSettings_Fields (useWatermark)(frequency)(opacity)
NX_REFLECTION_INSTRUMENT(WatermarkSettings, WatermarkSettings_Fields)

QN_FUSION_DECLARE_FUNCTIONS(WatermarkSettings, (json), NX_VMS_API)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::WatermarkSettings)
