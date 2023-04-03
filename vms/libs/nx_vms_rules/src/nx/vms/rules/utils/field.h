// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/assert.h>

#include "../manifest.h"
#include "../rules_fwd.h"

namespace nx::vms::rules::utils {

static constexpr auto kAcknowledgeFieldName = "acknowledge";
static constexpr auto kCameraIdFieldName = "cameraId";
static constexpr auto kDeviceIdsFieldName = "deviceIds";
static constexpr auto kDurationFieldName = "duration";
static constexpr auto kEmailsFieldName = "emails";
static constexpr auto kEngineIdFieldName = "engineId";
static constexpr auto kIntervalFieldName = "interval";
static constexpr auto kLayoutIdsFieldName = "layoutIds";
static constexpr auto kOmitLoggingFieldName = "omitLogging";
static constexpr auto kPlaybackTimeFieldName = "playbackTime";
static constexpr auto kRecordAfterFieldName = "recordAfter";
static constexpr auto kRecordBeforeFieldName = "recordBefore";
static constexpr auto kServerIdFieldName = "serverId";
static constexpr auto kSoundFieldName = "sound";
static constexpr auto kStateFieldName = "state";
static constexpr auto kTextFieldName = "text";
static constexpr auto kUsersFieldName = "users";

FieldDescriptor makeIntervalFieldDescriptor(
    const QString& displayName,
    const QString& description = {});

FieldDescriptor makeDurationFieldDescriptor(
    const QString& displayName,
    const QString& description = {});

FieldDescriptor makePlaybackFieldDescriptor(
    const QString& displayName,
    const QString& description = {});

FieldDescriptor makeStateFieldDescriptor(
    const QString& displayName,
    const QString& description = {},
    vms::rules::State defaultState = vms::rules::State::started);

FieldDescriptor makeExtractDetailFieldDescriptor(
    const QString& fieldName,
    const QString& detailName);

FieldDescriptor makeTextFormatterFieldDescriptor(
    const QString& fieldName,
    const QString& formatString);

FieldDescriptor makeTargetUserFieldDescriptor(
    const QString& displayName,
    const QString& description = {},
    bool isAvailableForAdminsByDefault = true);

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

NX_VMS_RULES_API QnUuidList getDeviceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API QnUuidList getResourceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API QnUuidList getResourceIds(const ActionPtr& action);

} // namespace nx::vms::rules::utils
