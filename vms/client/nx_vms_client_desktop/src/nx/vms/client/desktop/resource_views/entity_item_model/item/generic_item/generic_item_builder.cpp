// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_item_builder.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

GenericItemBuilder::GenericItemBuilder():
    m_constructedItem(std::make_unique<GenericItem>())
{
}

GenericItemBuilder& GenericItemBuilder::withRole(int role, const QVariant& value)
{
    m_constructedItem->m_roleDataProviders.emplace_back(role, value);
    return *this;
}

GenericItemBuilder& GenericItemBuilder::withRole(
    int role,
    const DataProvider& dataProvider,
    const InvalidatorPtr& invalidator)
{
    auto genericItem = m_constructedItem.get();

    if (invalidator)
        invalidator->addCallback([genericItem, role]{ genericItem->invalidateRole(role); });

    genericItem->m_roleDataProviders.emplace_back(role, dataProvider, invalidator);

    return *this;
}

GenericItemBuilder& GenericItemBuilder::withFlags(FlagsProvider flagsProvider)
{
    m_constructedItem->m_flagsProvider = flagsProvider;
    return *this;
}

GenericItemBuilder& GenericItemBuilder::withFlags(Qt::ItemFlags flags)
{
    m_constructedItem->m_flagsProvider = [flags] { return flags; };
    return *this;
}

GenericItemBuilder::operator AbstractItemPtr()
{
    if (!m_constructedItem)
    {
        NX_ASSERT(false, "Multiple call of GenericItemBuilder::operator(), null value returned");
        return AbstractItemPtr();
    }

    std::sort(std::begin(m_constructedItem->m_roleDataProviders),
        std::end(m_constructedItem->m_roleDataProviders));

    while (true)
    {
        auto itr = std::adjacent_find(std::begin(m_constructedItem->m_roleDataProviders),
            std::end(m_constructedItem->m_roleDataProviders));

        if (itr != std::end(m_constructedItem->m_roleDataProviders))
            itr = m_constructedItem->m_roleDataProviders.erase(itr);
        else
            break;
    }

    return std::move(m_constructedItem);
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
