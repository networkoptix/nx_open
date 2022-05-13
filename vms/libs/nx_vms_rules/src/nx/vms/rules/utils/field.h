// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_fields/optional_time_field.h"
#include "../manifest.h"

namespace nx::vms::rules::utils {

static constexpr auto kAggregationIntervalFieldName = "aggregationInterval";
static constexpr auto kCameraIdFieldName = "cameraId";
static constexpr auto kDeviceIdFieldName = "deviceId";
static constexpr auto kServerIdFieldName = "serverId";
static constexpr auto kEngineIdFieldName = "engineId";

inline FieldDescriptor makeAggregationIntervalFieldDescriptor(
    const QString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {})
{
    return makeFieldDescriptor<OptionalTimeField>(
        kAggregationIntervalFieldName,
        displayName,
        description,
        properties);
}

} // namespace nx::vms::rules::utils
