// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/extract_detail_field.h"
#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../aggregated_event.h"
#include "../basic_action.h"
#include "../event_filter_fields/state_field.h"

using namespace std::chrono_literals;

namespace nx::vms::rules::utils {

FieldDescriptor makeIntervalFieldDescriptor(
    const QString& displayName,
    const QString& description)
{
    return makeTimeFieldDescriptor<OptionalTimeField>(
        kIntervalFieldName,
        displayName,
        description,
        TimeFieldProperties{
            .value = 1min,
            .defaultValue = 1min,
            .minimumValue = 1s}.toVariantMap());
}

FieldDescriptor makePlaybackFieldDescriptor(
    const QString& displayName,
    const QString& description)
{
    return makeTimeFieldDescriptor<OptionalTimeField>(
        kPlaybackTimeFieldName,
        displayName,
        description,
        TimeFieldProperties{
            .value = 0s,
            .defaultValue = 1s,
            .maximumValue = 300s,
            .minimumValue = 0s}.toVariantMap());
}

FieldDescriptor makeStateFieldDescriptor(
    const QString& displayName,
    const QString& description,
    vms::rules::State defaultState)
{
    return makeFieldDescriptor<StateField>(
        kStateFieldName,
        displayName,
        description,
        {{"value", QVariant::fromValue(defaultState)}});
}

FieldDescriptor makeExtractDetailFieldDescriptor(
    const QString& fieldName,
    const QString& detailName)
{
    return makeFieldDescriptor<ExtractDetailField>(
        fieldName,
        fieldName,
        {},
        {{ "detailName", detailName }});
}

FieldDescriptor makeTextFormatterFieldDescriptor(
    const QString& fieldName,
    const QString& formatString)
{
    return makeFieldDescriptor<TextFormatter>(
        fieldName,
        fieldName,
        {},
        {{ "text", formatString }});
}

FieldDescriptor makeActionFlagFieldDescriptor(
    const QString& fieldName,
    const QString& displayName,
    const QString& description,
    bool defaultValue)
{
    return makeFieldDescriptor<ActionFlagField>(
        fieldName, displayName, description, {{"value", defaultValue}});
}

UuidList getDeviceIds(const AggregatedEventPtr& event)
{
    UuidList result;
    result << getFieldValue<nx::Uuid>(event, utils::kCameraIdFieldName);
    result << getFieldValue<UuidList>(event, utils::kDeviceIdsFieldName);
    result.removeAll(nx::Uuid());

    return result;
}

UuidList getResourceIds(const AggregatedEventPtr& event)
{
    auto result = getDeviceIds(event);
    result << getFieldValue<nx::Uuid>(event, kServerIdFieldName);
    result << getFieldValue<nx::Uuid>(event, kEngineIdFieldName);
    // TODO: #amalov Consider reporting user in resource list.
    result.removeAll(nx::Uuid());

    return result;
}

UuidList getResourceIds(const ActionPtr& action)
{
    UuidList result;
    result << getFieldValue<nx::Uuid>(action, kCameraIdFieldName);
    result << getFieldValue<UuidList>(action, kDeviceIdsFieldName);
    result << getFieldValue<nx::Uuid>(action, kServerIdFieldName);
    result.removeAll(nx::Uuid());

    return result;
}

} // namespace nx::vms::rules::utils
