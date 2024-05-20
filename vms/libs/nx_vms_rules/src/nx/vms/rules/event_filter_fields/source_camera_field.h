// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/resource_filter_field.h"

namespace nx::vms::rules {

constexpr auto kCameraAnalyticsValidationPolicy = "cameraAnalytics";
constexpr auto kCameraInputValidationPolicy = "cameraInput";
constexpr auto kCameraMotionValidationPolicy = "cameraMotion";

class NX_VMS_RULES_API SourceCameraField: public ResourceFilterEventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.device")

public:
    using ResourceFilterEventField::ResourceFilterEventField;
};

} // namespace nx::vms::rules
