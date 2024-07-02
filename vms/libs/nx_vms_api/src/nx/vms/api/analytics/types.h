// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QRectF>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct Rect
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};
#define nx_vms_api_analytics_Rect_Fields (x)(y)(width)(height)
QN_FUSION_DECLARE_FUNCTIONS(Rect, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(Rect, nx_vms_api_analytics_Rect_Fields)

} // namespace nx::vms::api::analytics
