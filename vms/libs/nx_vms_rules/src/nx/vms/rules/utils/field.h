// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/assert.h>
#include <nx/utils/uuid.h>

#include "../field.h"
#include "../manifest.h"
#include "../rules_fwd.h"

namespace nx::vms::rules {

template<class T>
FieldDescriptor makeFieldDescriptor(
    const QString& fieldName,
    const QString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {})
{
    return FieldDescriptor{
        .id = fieldMetatype<T>(),
        .fieldName = fieldName,
        .displayName = displayName,
        .description = description,
        .properties = properties
    };
}

namespace utils {

static constexpr auto kAcknowledgeFieldName = "acknowledge";
static constexpr auto kAttributesFieldName = "attributes";
static constexpr auto kCameraIdFieldName = "cameraId";
static constexpr auto kCaptionFieldName = "caption";
static constexpr auto kDescriptionFieldName = "description";
static constexpr auto kDeviceIdsFieldName = "deviceIds";
static constexpr auto kDurationFieldName = "duration";
static constexpr auto kEmailsFieldName = "emails";
static constexpr auto kEventTypeIdFieldName = "eventTypeId";
static constexpr auto kEngineIdFieldName = "engineId";
static constexpr auto kIdFieldName = "id";
static constexpr auto kIntervalFieldName = "interval";
static constexpr auto kLayoutIdFieldName = "layoutId";
static constexpr auto kLayoutIdsFieldName = "layoutIds";
static constexpr auto kObjectTypeIdFieldName = "objectTypeId";
static constexpr auto kOmitLoggingFieldName = "omitLogging";
static constexpr auto kPlaybackTimeFieldName = "playbackTime";
static constexpr auto kRecordAfterFieldName = "recordAfter";
static constexpr auto kRecordBeforeFieldName = "recordBefore";
static constexpr auto kServerIdFieldName = "serverId";
static constexpr auto kServerIdsFieldName = "serverIds";
static constexpr auto kSoundFieldName = "sound";
static constexpr auto kSourceFieldName = "source";
static constexpr auto kStateFieldName = "state";
static constexpr auto kTextFieldName = "text";
static constexpr auto kUsersFieldName = "users";
static constexpr auto kUserIdFieldName = "userId";

template <class T>
FieldDescriptor makeTimeFieldDescriptor(
    const QString& fieldName,
    const QString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {})
{
    return makeFieldDescriptor<T>(
        fieldName,
        displayName,
        description,
        properties);
}

NX_VMS_RULES_API FieldDescriptor makeIntervalFieldDescriptor(
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

FieldDescriptor makeActionFlagFieldDescriptor(
    const QString& fieldName,
    const QString& displayName,
    const QString& description = {},
    bool defaultValue = false);

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

// TODO: #amalov Consider moving to resource.h.
NX_VMS_RULES_API UuidList getDeviceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API UuidList getResourceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API UuidList getResourceIds(const ActionPtr& action);

} // namespace utils
} // namespace nx::vms::rules
