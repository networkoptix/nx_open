#include "radass_resource_manager.h"

#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_index.h>

#include <nx/client/desktop/radass/radass_types.h>

#include <nx/utils/algorithm/same.h>
#include <nx/utils/uuid.h>

namespace {

QnUuid id(const QnLayoutResourcePtr& layout)
{
    return layout->getId();
}

QnUuid id(const QnLayoutItemIndex& index)
{
    return index.uuid();
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct RadassResourceManager::Private
{
    RadassMode mode(const QnUuid& key)
    {
        return m_modes.value(key, RadassMode::Auto);
    }

    void setMode(const QnUuid& key, RadassMode value)
    {
        if (value == RadassMode::Auto)
            m_modes.remove(key);
        else
            m_modes[key] = value;
    }

private:
    //we need to store cameras only!!!!!11111111
    //what's with items removed from layout? should we store layout id too? and system id?
    QHash<QnUuid, RadassMode> m_modes;
};

RadassResourceManager::RadassResourceManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

RadassResourceManager::~RadassResourceManager()
{
}

RadassMode RadassResourceManager::mode(const QnLayoutResourcePtr& layout) const
{
    if (!layout)
        return RadassMode::Auto;

    // invalid!!! need calculate mode for separate items if it differs

    return d->mode(id(layout));
}

void RadassResourceManager::setMode(const QnLayoutResourcePtr& layout, RadassMode value)
{
    if (!layout)
        return;

    // need to clear all item's mode

    d->setMode(id(layout), value);
}

RadassMode RadassResourceManager::mode(const QnLayoutItemIndexList& items) const
{
    NX_ASSERT(!items.empty());
    if (items.empty())
        return RadassMode::Auto;

    auto value = RadassMode::Auto;
    if (nx::utils::algorithm::same(items.cbegin(), items.cend(),
        [this](const QnLayoutItemIndex& index)
        {
            return d->mode(id(index));
        },
        &value))
    {
        return value;
    }

    return RadassMode::Custom;
}

void RadassResourceManager::setMode(const QnLayoutItemIndexList& items, RadassMode value)
{
    //set mode for items must update their layout's mode (e.g to Custom)

    for (const auto& item: items)
        d->setMode(id(item), value);
}

} // namespace desktop
} // namespace client
} // namespace nx
