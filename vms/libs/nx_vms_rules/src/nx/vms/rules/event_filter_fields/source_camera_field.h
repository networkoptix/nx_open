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
    Q_CLASSINFO("metatype", "device")

public:
    using ResourceFilterEventField::ResourceFilterEventField;
    static QJsonObject openApiDescriptor(const QVariantMap& properties)
    {
        auto descriptor = ResourceFilterEventField::openApiDescriptor(properties);
        auto resourceFilterProperties = ResourceFilterFieldProperties::fromVariantMap(properties);
        QString description =
            "Defines the source camera resources to be used for event filtering.";

        if (resourceFilterProperties.acceptAll && resourceFilterProperties.allowEmptySelection)
            description += "<br/>If left unspecified or empty, events from any camera will be considered.";

        descriptor[utils::kDescriptionProperty] = description;
        return descriptor;
    }
};

} // namespace nx::vms::rules
