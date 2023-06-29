// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "field.h"

namespace nx::vms::rules::utils {

/** Returns whether logging is omitted for the given descriptor and corresponding item. */
template<typename ItemPtr>
bool isLoggingAllowed(const ItemDescriptor& descriptor, const ItemPtr& item)
{
    static_assert(
        std::is_same<EventPtr, ItemPtr>::value || std::is_same<ActionPtr, ItemPtr>::value,
        "ItemPtr must be EventPtr or ActionPtr");

    if (descriptor.flags.testFlag(ItemFlag::omitLogging))
        return false;

    const auto property = item->property(utils::kOmitLoggingFieldName);
    if (property.isValid())
        return !property.template value<bool>();

    return true;
}

inline bool aggregateByType(
    const ItemDescriptor& eventDescriptor,
    const ItemDescriptor& actionDescriptor)
{
    return actionDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported)
        && eventDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported);
}

} // namespace nx::vms::rules::utils
