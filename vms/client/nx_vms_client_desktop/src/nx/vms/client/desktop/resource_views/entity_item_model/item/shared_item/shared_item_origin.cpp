// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_item_origin.h"

#include <nx/vms/client/desktop/resource_views/entity_item_model/item/shared_item/shared_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

SharedItemOrigin::SharedItemOrigin(std::unique_ptr<AbstractItem> itemData):
    m_sourceItemData(std::move(itemData))
{
    m_sourceItemData->setDataChangedCallback(
        [this](const QVector<int>& roles) { onSourceDataChanged(roles); });
};

SharedItemOrigin::~SharedItemOrigin()
{
    m_sharedInstanceCountObserver = {};
}

std::unique_ptr<AbstractItem> SharedItemOrigin::createSharedInstance()
{
    auto result = std::make_unique<SharedItem>(shared_from_this());
    m_sharedInstances.insert(result.get());
    if (m_sharedInstanceCountObserver)
        m_sharedInstanceCountObserver(m_sharedInstances.size());
    return result;
}

void SharedItemOrigin::onSourceDataChanged(const QVector<int>& roles)
{
    for (const SharedItem* instance: m_sharedInstances)
        instance->notifyDataChanged(roles);
}

QVariant SharedItemOrigin::data(int role) const
{
    return m_sourceItemData->data(role);
}

Qt::ItemFlags SharedItemOrigin::flags() const
{
    return m_sourceItemData->flags();
}

void SharedItemOrigin::setSharedInstanceCountObserver(SharedInstanceCountObserver observer)
{
    m_sharedInstanceCountObserver = observer;
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
