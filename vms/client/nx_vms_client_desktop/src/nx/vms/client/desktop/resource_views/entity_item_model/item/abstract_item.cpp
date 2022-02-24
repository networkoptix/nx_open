// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_item.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {

AbstractItem::AbstractItem()
{
}

AbstractItem::~AbstractItem()
{
}

void AbstractItem::notifyDataChanged(const QVector<int>& roles) const
{
    if (m_dataChangedCallback)
        m_dataChangedCallback(roles);
}

void AbstractItem::setDataChangedCallback(const DataChangedCallback& callback)
{
    m_dataChangedCallback = callback;
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
