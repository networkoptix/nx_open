// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "compatibility.h"

#include <algorithm>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/translatable_string.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/engine.h>

namespace nx::vms::rules::utils {

using namespace nx::vms::common;

namespace {

using DescriptorFinder =
    std::function<std::optional<ItemDescriptor>(const SystemContext* context, const QString& id)>;

bool isCompatible(
    const SystemContext* context, const ItemFilters& filters, const ItemDescriptor& descriptor)
{
    return std::all_of(
        filters.begin(),
        filters.end(),
        [context, &descriptor](const auto& filter) { return filter(context, descriptor); });
}

void filterInplace(
    const SystemContext* context,
    const ItemFilters& filters,
    const DescriptorFinder& finder,
    Group& group)
{
    auto it = group.items.begin();
    while (it != group.items.end())
    {
        const auto descriptor = finder(context, *it);

        if (!descriptor || !isCompatible(context, filters, *descriptor))
            it = group.items.erase(it);
        else
             ++it;
    }
}

} // namespace

bool hasItemSupportedServer(
    const SystemContext* context, const ItemDescriptor& itemDescriptor)
{
    if (itemDescriptor.serverFlags == 0)
        return true;

    for (const auto& server: context->resourcePool()->servers())
    {
        if (server->getServerFlags().testFlags(itemDescriptor.serverFlags))
            return true;
    }

    return false;
}

bool isItemSupportedLicense(
    const SystemContext* context, const ItemDescriptor& manifest)
{
    using namespace nx::vms::common::saas;

    if (manifest.flags.testFlag(ItemFlag::saasLicense) && !saasInitialized(context))
        return false;

    if (manifest.flags.testFlag(ItemFlag::localLicense) && saasInitialized(context))
        return false;

    return true;
}

bool isItemActual(
    const SystemContext* /*context*/, const ItemDescriptor& manifest)
{
    return !manifest.flags.testFlag(ItemFlag::deprecated);
}

bool isItemExternal(
    const SystemContext* /*context*/, const ItemDescriptor& manifest)
{
    return !manifest.flags.testFlag(ItemFlag::system);
}

ItemFilters defaultItemFilters()
{
    return {
        hasItemSupportedServer,
        isItemSupportedLicense,
        isItemActual,
        isItemExternal,
    };
}

QList<ItemDescriptor> filterItems(
    const SystemContext* context,
    const ItemFilters& filters,
    QList<ItemDescriptor> descriptors)
{
    auto it = descriptors.begin();
    while (it != descriptors.end())
    {
        if (isCompatible(context, filters, *it))
            ++it;
        else
            it = descriptors.erase(it);
    }

    return descriptors;
}

QVector<ItemDescriptor> sortItems(QList<ItemDescriptor> descriptors)
{
    std::sort(descriptors.begin(),descriptors.end(),
        [](const auto& left, const auto& right)
        {
            return left.displayName < right.displayName;
        });
    return descriptors;
}

Group filterEventGroups(
    const SystemContext* context,
    const ItemFilters& filters,
    Group group)
{
    auto finder =
        [](const SystemContext* context, const QString& id)
        {
            return context->vmsRulesEngine()->eventDescriptor(id);
        };

    filterInplace(context, filters, finder, group);
    return group;
}

Group filterActionGroups(
    const SystemContext* context,
    const ItemFilters& filters,
    Group group)
{
    auto finder =
        [](const SystemContext* context, const QString& id)
        {
            return context->vmsRulesEngine()->actionDescriptor(id);
        };

    filterInplace(context, filters, finder, group);
    return group;
}

} // namespace nx::vms::rules::utils
