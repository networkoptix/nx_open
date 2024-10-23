// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_lookup_field_validator.h"

#include <nx/vms/common/lookup_lists/lookup_list_manager.h>

#include "../event_filter_fields/text_lookup_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult TextLookupFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* context) const
{
    auto textLookupField = dynamic_cast<const TextLookupField*>(field);
    if (!NX_ASSERT(textLookupField))
        return {QValidator::State::Invalid, {Strings::invalidFieldType()}};

    if (textLookupField->checkType() == TextLookupCheckType::inList
        || textLookupField->checkType() == TextLookupCheckType::notInList)
    {
        const auto fieldValue = textLookupField->value();
        if (fieldValue.isEmpty())
            return {QValidator::State::Invalid, tr("List is not selected")};

        const auto lookupList = context->lookupListManager()->lookupList(Uuid{fieldValue});
        if (lookupList == nx::vms::api::LookupListData{})
        {
            return {
                QValidator::State::Invalid,
                tr("List with the given id '%1' does not exist").arg(fieldValue)};
        }

        if (!lookupList.objectTypeId.isEmpty())
        {
            return {
                QValidator::State::Invalid,
                tr("List with the given id '%1' is not a generic list").arg(fieldValue)};
        }
    }

    return {};
}

} // namespace nx::vms::rules
