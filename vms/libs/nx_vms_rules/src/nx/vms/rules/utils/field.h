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
    const QVariantMap& properties = {},
    const QStringList& linkedFields = {})
{
    return FieldDescriptor{
        .id = fieldMetatype<T>(),
        .fieldName = fieldName,
        .displayName = displayName,
        .description = description,
        .properties = properties,
        .linkedFields = linkedFields
    };
}

namespace utils {

static constexpr auto kAcknowledgeFieldName = "acknowledge";
static constexpr auto kCameraIdFieldName = "cameraId";
static constexpr auto kCaptionFieldName = "caption";
static constexpr auto kDescriptionFieldName = "description";
static constexpr auto kDeviceIdsFieldName = "deviceIds";
static constexpr auto kDurationFieldName = "duration";
static constexpr auto kEmailsFieldName = "emails";
static constexpr auto kEngineIdFieldName = "engineId";
static constexpr auto kIntervalFieldName = "interval";
static constexpr auto kLayoutIdsFieldName = "layoutIds";
static constexpr auto kObjectTypeIdFieldName = "objectTypeId";
static constexpr auto kOmitLoggingFieldName = "omitLogging";
static constexpr auto kPlaybackTimeFieldName = "playbackTime";
static constexpr auto kRecordAfterFieldName = "recordAfter";
static constexpr auto kRecordBeforeFieldName = "recordBefore";
static constexpr auto kServerIdFieldName = "serverId";
static constexpr auto kServerIdsFieldName = "serverIds";
static constexpr auto kSoundFieldName = "sound";
static constexpr auto kStateFieldName = "state";
static constexpr auto kTextFieldName = "text";
static constexpr auto kUsersFieldName = "users";

enum class UserFieldPreset
{
    None,
    All,
    Power, // Owners and administrators.
};

struct TimeFieldProperties
{
    /* Value that used by the action builder to make a field. */
    std::chrono::seconds initialValue = std::chrono::seconds::zero();

    /* Value that used by the OptionalTimeField on switching from 'no duration' to 'has duration'. */
    std::chrono::seconds defaultValue = initialValue;

    /* Maximum duration value. */
    std::chrono::seconds maximumValue = std::chrono::weeks{1};

    /* Minimum duration value. */
    std::chrono::seconds minimumValue = std::chrono::seconds::zero();
};

template <class T>
FieldDescriptor makeTimeFieldDescriptor(
    const QString& fieldName,
    const QString& displayName,
    const QString& description = {},
    const TimeFieldProperties& properties = {},
    const QStringList& linkedFields = {})
{
    return makeFieldDescriptor<T>(
        fieldName,
        displayName,
        description,
        {
            {"value", QVariant::fromValue(properties.initialValue)},
            {"default", QVariant::fromValue(properties.defaultValue)},
            {"min", QVariant::fromValue(properties.minimumValue)},
            {"max", QVariant::fromValue(properties.maximumValue)}
        },
        linkedFields);
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

FieldDescriptor makeTargetUserFieldDescriptor(
    const QString& displayName,
    const QString& description = {},
    UserFieldPreset preset = UserFieldPreset::Power,
    bool visible = true,
    const QStringList& linkedFields = {});

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
NX_VMS_RULES_API QnUuidList getDeviceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API QnUuidList getResourceIds(const AggregatedEventPtr& event);
NX_VMS_RULES_API QnUuidList getResourceIds(const ActionPtr& action);

} // namespace utils
} // namespace nx::vms::rules
