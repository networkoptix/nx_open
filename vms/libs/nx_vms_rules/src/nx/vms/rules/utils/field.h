// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_fields/optional_time_field.h"
#include "../event_fields/state_field.h"
#include "../manifest.h"

namespace nx::vms::rules::utils {

static constexpr auto kCameraIdFieldName = "cameraId";
static constexpr auto kDurationFieldName = "duration";
static constexpr auto kEngineIdFieldName = "engineId";
static constexpr auto kIntervalFieldName = "interval";
static constexpr auto kServerIdFieldName = "serverId";
static constexpr auto kStateFieldName = "state";
static constexpr auto kUsersFieldName = "users";

inline FieldDescriptor makeIntervalFieldDescriptor(
    const QString& displayName,
    const QString& description = {})
{
    return makeFieldDescriptor<OptionalTimeField>(
        kIntervalFieldName,
        displayName,
        description,
        {});
}

inline FieldDescriptor makeStateFieldDescriptor(
    const QString& displayName,
    const QString& description = {})
{
    return makeFieldDescriptor<StateField>(
        kStateFieldName,
        displayName,
        description,
        {});
}

} // namespace nx::vms::rules::utils
