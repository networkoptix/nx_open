#include "radass_resource_manager.h"

#include <core/resource/layout_resource.h>
#include <core/resource/layout_item_index.h>

#include <nx/client/desktop/radass/radass_types.h>

#include <nx/utils/algorithm/same.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

struct RadassResourceManager::Private
{
    RadassMode mode(const QnLayoutItemIndex& index)
    {
        return m_modes.value(index.uuid(), RadassMode::Auto);
    }

    void setMode(const QnLayoutItemIndex& index, RadassMode value)
    {
        NX_ASSERT(value != RadassMode::Custom);
        if (value == RadassMode::Auto)
            m_modes.remove(index.uuid());
        else
            m_modes[index.uuid()] = value;
    }

private:
    // Mode by layout item uuid.
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

    QnLayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << QnLayoutItemIndex(layout, item.uuid);

    return mode(items);
}

void RadassResourceManager::setMode(const QnLayoutResourcePtr& layout, RadassMode value)
{
    if (!layout)
        return;

    // Custom mode is not to be set directly.
    if (value == RadassMode::Custom)
        return;

    QnLayoutItemIndexList items;
    for (const auto item: layout->getItems())
        items << QnLayoutItemIndex(layout, item.uuid);

    setMode(items, value);
}

RadassMode RadassResourceManager::mode(const QnLayoutItemIndexList& items) const
{
    auto value = RadassMode::Auto;
    if (nx::utils::algorithm::same(items.cbegin(), items.cend(),
        [this](const QnLayoutItemIndex& index)
        {
            return d->mode(index);
        },
        &value))
    {
        return value;
    }

    return RadassMode::Custom;
}

void RadassResourceManager::setMode(const QnLayoutItemIndexList& items, RadassMode value)
{
    // Custom mode is not to be set directly.
    if (value == RadassMode::Custom)
        return;

    for (const auto& item: items)
        d->setMode(item, value);
}

} // namespace desktop
} // namespace client
} // namespace nx
