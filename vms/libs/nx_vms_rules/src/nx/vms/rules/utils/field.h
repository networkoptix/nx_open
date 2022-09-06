// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../manifest.h"
#include "../rules_fwd.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::rules::utils {

static constexpr auto kCameraIdFieldName = "cameraId";
static constexpr auto kDeviceIdsFieldName = "deviceIds";
static constexpr auto kDurationFieldName = "duration";
static constexpr auto kEngineIdFieldName = "engineId";
static constexpr auto kIntervalFieldName = "interval";
static constexpr auto kServerIdFieldName = "serverId";
static constexpr auto kStateFieldName = "state";
static constexpr auto kUsersFieldName = "users";

FieldDescriptor makeIntervalFieldDescriptor(
    const QString& displayName,
    const QString& description = {});

FieldDescriptor makeStateFieldDescriptor(
    const QString& displayName,
    const QString& description = {});

FieldDescriptor makeExtractDetailFieldDescriptor(
    const QString& fieldName,
    const QString& detailName);

template <class T, class E>
T getFieldValue(const E& event, const char* fieldName, T&& defaultValue = T())
{
    if (!NX_ASSERT(event))
        return std::forward<T>(defaultValue);

    const QVariant value = event->property(fieldName);
    if (!value.isValid() || !value.canConvert<T>())
        return std::forward<T>(defaultValue);

    return value.value<T>();
}

} // namespace nx::vms::rules::utils
