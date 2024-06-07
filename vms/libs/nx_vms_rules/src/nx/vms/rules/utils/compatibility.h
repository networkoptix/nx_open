// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "../group.h"
#include "../manifest.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules::utils {

using ItemFilter =
    std::function<bool(const nx::vms::common::SystemContext*, const ItemDescriptor&)>;

using ItemFilters = QList<ItemFilter>;

/** Returns whether any of the given servers support given event or action. */
NX_VMS_RULES_API bool hasItemSupportedServer(
    const nx::vms::common::SystemContext* context, const ItemDescriptor& itemDescriptor);

NX_VMS_RULES_API bool isItemSupportedLicense(
    const nx::vms::common::SystemContext* context, const ItemDescriptor& manifest);

NX_VMS_RULES_API bool isItemActual(
    const nx::vms::common::SystemContext* context, const ItemDescriptor& manifest);

NX_VMS_RULES_API bool isItemExternal(
    const nx::vms::common::SystemContext* context, const ItemDescriptor& manifest);

NX_VMS_RULES_API bool isItemInternal(
    const nx::vms::common::SystemContext* context, const ItemDescriptor& manifest);

NX_VMS_RULES_API ItemFilters defaultItemFilters();

/** Filter manifest with predicate list. */
NX_VMS_RULES_API QList<ItemDescriptor> filterItems(
    const nx::vms::common::SystemContext* context,
    const ItemFilters& filters,
    QList<ItemDescriptor> descriptors);

NX_VMS_RULES_API Group filterEventGroups(
    const nx::vms::common::SystemContext* context,
    const ItemFilters& filters,
    Group group);

NX_VMS_RULES_API Group filterActionGroups(
    const nx::vms::common::SystemContext* context,
    const ItemFilters& filters,
    Group group);

} // namespace nx::vms::rules::utils
