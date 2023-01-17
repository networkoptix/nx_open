// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include "../action_builder_fields/extract_detail_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../aggregated_event.h"
#include "../event_filter_fields/state_field.h"

namespace nx::vms::rules::utils {

FieldDescriptor makeIntervalFieldDescriptor(
    const QString& displayName,
    const QString& description)
{
    return makeFieldDescriptor<OptionalTimeField>(
        kIntervalFieldName,
        displayName,
        description,
        {});
}

FieldDescriptor makeDurationFieldDescriptor(
    const QString& displayName,
    const QString& description)
{
    return makeFieldDescriptor<OptionalTimeField>(
        kDurationFieldName,
        displayName,
        description,
        {});
}

FieldDescriptor makeStateFieldDescriptor(
    const QString& displayName,
    const QString& description)
{
    return makeFieldDescriptor<StateField>(
        kStateFieldName,
        displayName,
        description,
        {});
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

QnUuidList getDeviceIds(const AggregatedEventPtr& event)
{
    QnUuidList result;
    result << getFieldValue<QnUuid>(event, utils::kCameraIdFieldName);
    result << getFieldValue<QnUuidList>(event, utils::kDeviceIdsFieldName);
    result.removeAll(QnUuid());

    return result;
}

} // namespace nx::vms::rules::utils
