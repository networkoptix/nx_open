// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_lookup_field_validator.h"

#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/field_names.h>

#include "../event_filter_fields/object_lookup_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult ObjectLookupFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto objectLookupField = dynamic_cast<const ObjectLookupField*>(field);
    if (!NX_ASSERT(objectLookupField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    if (objectLookupField->checkType() != ObjectLookupCheckType::hasAttributes)
    {
        const auto fieldValue = objectLookupField->value();
        if (fieldValue.isEmpty())
            return {QValidator::State::Invalid, tr("List is not selected")};

        const auto lookupList = context->lookupListManager()->lookupList(Uuid{fieldValue});
        if (lookupList == nx::vms::api::LookupListData{})
        {
            return {
                QValidator::State::Invalid,
                tr("List with the given id '%1' does not exist").arg(fieldValue)};
        }

        auto analyticsObjectTypeField =
            rule->eventFilters().first()->fieldByName<AnalyticsObjectTypeField>(
                utils::kObjectTypeIdFieldName);
        if (!NX_ASSERT(analyticsObjectTypeField))
        {
            return {
                QValidator::State::Invalid,
                Strings::fieldValueMustBeProvided(utils::kObjectTypeIdFieldName)};
        }

        if (analyticsObjectTypeField->value() != lookupList.objectTypeId)
        {
            return {
                QValidator::State::Invalid,
                tr("List with the given id '%1' has invalid object type - '%2', expected - '%3'")
                    .arg(fieldValue)
                    .arg(lookupList.objectTypeId)
                    .arg(analyticsObjectTypeField->value())};
        }
    }

    return {};
}

} // namespace nx::vms::rules
