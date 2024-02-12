// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_aggregator.h"

#include <core/resource/layout_resource.h>

using namespace nx::vms::common;

QnLayoutItemAggregator::QnLayoutItemAggregator(QObject* parent):
    base_type(parent)
{

}

QnLayoutItemAggregator::~QnLayoutItemAggregator()
{

}

bool QnLayoutItemAggregator::addWatchedLayout(const QnLayoutResourcePtr& layout)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_watchedLayouts.contains(layout))
        return false;

    m_watchedLayouts.insert(layout);

    std::vector<nx::Uuid> added;
    for (const auto& item: layout->getItems())
    {
        if (!item.resource.id.isNull() && m_items.insert(item.resource.id))
            added.push_back(item.resource.id);
    }

    connect(layout.get(), &QnLayoutResource::itemAdded, this,
        [this](const QnLayoutResourcePtr& /*layout*/, const LayoutItemData& item)
        {
            handleItemAdded(item);
        }, Qt::DirectConnection);

    connect(layout.get(), &QnLayoutResource::itemRemoved, this,
        [this](const QnLayoutResourcePtr& /*layout*/, const LayoutItemData& item)
        {
            handleItemRemoved(item);
        }, Qt::DirectConnection);

    lock.unlock();
    for (auto id: added)
        emit itemAdded(id);

    return true;
}

bool QnLayoutItemAggregator::removeWatchedLayout(const QnLayoutResourcePtr& layout)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!m_watchedLayouts.contains(layout))
        return false;

    m_watchedLayouts.remove(layout);

    layout->disconnect(this);

    std::vector<nx::Uuid> removed;
    for (const auto& item: layout->getItems())
    {
        if (!item.resource.id.isNull() && m_items.remove(item.resource.id))
            removed.push_back(item.resource.id);
    }

    lock.unlock();
    for (auto id: removed)
        emit itemRemoved(id);

    return true;
}

QSet<QnLayoutResourcePtr> QnLayoutItemAggregator::watchedLayouts() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_watchedLayouts;
}

bool QnLayoutItemAggregator::hasLayout(const QnLayoutResourcePtr& layout) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_watchedLayouts.contains(layout);
}

bool QnLayoutItemAggregator::hasItem(const nx::Uuid& id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_items.contains(id);
}

void QnLayoutItemAggregator::handleItemAdded(const LayoutItemData& item)
{
    const auto resourceId = item.resource.id;
    if (resourceId.isNull())
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_items.insert(resourceId))
    {
        lock.unlock();
        emit itemAdded(resourceId);
    }
}

void QnLayoutItemAggregator::handleItemRemoved(const LayoutItemData& item)
{
    const auto resourceId = item.resource.id;
    if (resourceId.isNull())
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_items.remove(resourceId))
    {
        lock.unlock();
        emit itemRemoved(resourceId);
    }
}
