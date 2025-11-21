// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "radass_resource_manager.h"

#include <core/resource_management/resource_pool.h>
#include <nx/utils/algorithm/same.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/radass/radass_support.h>
#include <nx/vms/client/desktop/resource/layout_item_index.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/settings/user_specific_settings.h>

namespace nx::vms::client::desktop {

static const RadassMode kDefaultResolution = nx::reflect::fromString(ini().defaultResolution,
    RadassMode::Auto);

namespace {

RadassModeByLayoutItemIdHash removeNonexistentItems(
    RadassModeByLayoutItemIdHash source, QnResourcePool* resourcePool)
{
    if (!NX_ASSERT(resourcePool))
        return source;

    UuidSet existingItems;
    for (const auto& layout: resourcePool->getResources<core::LayoutResource>())
        existingItems.unite(nx::utils::toQSet(layout->getItems().keys()));

    const auto storedItems = nx::utils::toQSet(source.keys());

    auto removedItems = storedItems - existingItems;
    for (const auto& item: removedItems)
        source.remove(item);

    return source;
}

} // namespace

RadassResourceManager::RadassResourceManager(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext)
{
    if (!NX_ASSERT(systemContext))
        return;

    connect(systemContext, &SystemContext::remoteIdChanged, this,
        [this, systemContext]()
        {
            auto crossSiteItemModes =
                systemContext->userSettings()->crossSiteLayoutItemRadassModes();
            systemContext->userSettings()->crossSiteLayoutItemRadassModes = removeNonexistentItems(
                crossSiteItemModes, nx::vms::client::core::appContext()->cloudLayoutsPool());

            auto localItemModes = systemContext->localSettings()->localLayoutItemRadassModes();
            systemContext->localSettings()->localLayoutItemRadassModes = removeNonexistentItems(
                localItemModes, systemContext->resourcePool());
        });
}

RadassResourceManager::~RadassResourceManager()
{
}

RadassMode RadassResourceManager::mode(const core::LayoutResourcePtr& layout) const
{
    if (!layout)
        return RadassMode::Auto;

    LayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << LayoutItemIndex(layout, item.uuid);

    return mode(items);
}

void RadassResourceManager::setMode(const core::LayoutResourcePtr& layout, RadassMode value)
{
    if (!layout)
        return;

    // Custom mode is not to be set directly.
    if (value == RadassMode::Custom)
        return;

    LayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << LayoutItemIndex(layout, item.uuid);

    setMode(items, value);
}

RadassMode RadassResourceManager::mode(const LayoutItemIndex& item) const
{
    return mode(LayoutItemIndexList() << item);
}

void RadassResourceManager::setMode(const LayoutItemIndex& item, RadassMode value)
{
    setMode(LayoutItemIndexList() << item, value);
}

RadassMode RadassResourceManager::mode(const LayoutItemIndexList& items) const
{
    LayoutItemIndexList validItems;
    for (const auto& item: items)
    {
        if (isRadassSupported(item))
            validItems.push_back(item);
    }

    auto value = RadassMode::Auto;
    if (nx::utils::algorithm::same(validItems.cbegin(), validItems.cend(),
        [this](const LayoutItemIndex& index)
        {
            const bool isCloudLayout =
                (bool) index.layout().dynamicCast<core::CrossSystemLayoutResource>();
            RadassModeByLayoutItemIdHash modes = isCloudLayout
                ? systemContext()->userSettings()->crossSiteLayoutItemRadassModes()
                : systemContext()->localSettings()->localLayoutItemRadassModes();

            return modes.value(index.uuid(), kDefaultResolution);
        },
        &value))
    {
        return value;
    }

    return RadassMode::Custom;
}

void RadassResourceManager::setMode(const LayoutItemIndexList& items, RadassMode value)
{
    // Custom mode is not to be set directly.
    if (value == RadassMode::Custom)
        return;

    for (const auto& item: items)
    {
        if (!isRadassSupported(item))
            continue;

        auto oldMode = mode(item);

        const bool isCloudLayout =
            (bool) item.layout().dynamicCast<core::CrossSystemLayoutResource>();
        const bool removeIfAuto =
            value == RadassMode::Auto && kDefaultResolution == RadassMode::Auto;

        if (isCloudLayout)
        {
            auto itemModes = systemContext()->userSettings()->crossSiteLayoutItemRadassModes();
            if (removeIfAuto)
                itemModes.remove(item.uuid());
            else
                itemModes[item.uuid()] = value;
            systemContext()->userSettings()->crossSiteLayoutItemRadassModes = itemModes;
        }
        else
        {
            auto itemModes = systemContext()->localSettings()->localLayoutItemRadassModes();
            if (removeIfAuto)
                itemModes.remove(item.uuid());
            else
                itemModes[item.uuid()] = value;
            systemContext()->localSettings()->localLayoutItemRadassModes = itemModes;
        }

        if (oldMode != value)
            emit modeChanged(item, value);
    }
}

} // namespace nx::vms::client::desktop
