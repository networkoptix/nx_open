// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include "../action_fields/extract_detail_field.h"
#include "../action_fields/optional_time_field.h"
#include "../event_fields/state_field.h"

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

} // namespace nx::vms::rules::utils
