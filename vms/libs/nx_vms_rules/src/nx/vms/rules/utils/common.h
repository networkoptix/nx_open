// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

#include "../manifest.h"
#include "../rules_fwd.h"

namespace nx::vms::rules::utils {

inline bool aggregateByType(
    const ItemDescriptor& eventDescriptor,
    const ItemDescriptor& actionDescriptor)
{
    return actionDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported)
        && eventDescriptor.flags.testFlag(ItemFlag::aggregationByTypeSupported);
}

inline bool isInstantOnly(const ItemDescriptor& descriptor)
{
    return descriptor.flags.testFlag(vms::rules::ItemFlag::instant)
        && !descriptor.flags.testFlags(vms::rules::ItemFlag::prolonged);
}

/**
 * Returns field descriptor with the given field name from the given field descriptor or
 * std::nullopt if there is no such field.
 */
NX_VMS_RULES_API std::optional<FieldDescriptor> fieldByName(
    const QString& fieldName, const ItemDescriptor& descriptor);

/** Returns whether logging is allowed for the given rule. */
NX_VMS_RULES_API bool isLoggingAllowed(const Engine* engine, QnUuid ruleId);

/** Returns whether any of the given servers support given event or action. */
NX_VMS_RULES_API bool hasItemSupportedServer(
    const QnMediaServerResourceList& servers, const ItemDescriptor& itemDescriptor);

} // namespace nx::vms::rules::utils
