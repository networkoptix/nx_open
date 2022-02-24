// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_item.h"
#include "shared_item_origin.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {

SharedItem::SharedItem(const std::shared_ptr<SharedItemOrigin>& sharedDataHolder):
    base_type(),
    m_sharedDataHolder(sharedDataHolder)
{
}

SharedItem::~SharedItem()
{
    if (auto shared = m_sharedDataHolder.lock())
    {
        shared->m_sharedInstances.erase(this);
        if (shared->m_sharedInstanceCountObserver)
            shared->m_sharedInstanceCountObserver(shared->m_sharedInstances.size());
    }
}

QVariant SharedItem::data(int role) const
{
    if (auto shared = m_sharedDataHolder.lock())
        return shared->data(role);

    return QVariant();
}

Qt::ItemFlags SharedItem::flags() const
{
    if (auto shared = m_sharedDataHolder.lock())
        return shared->flags();

    return Qt::NoItemFlags;
}

void SharedItem::notifyDataChanged(const QVector<int>& roles) const
{
    base_type::notifyDataChanged({roles});
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
