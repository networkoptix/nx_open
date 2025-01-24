// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_watcher.h"

#include <QtCore/QHash>
#include <QtCore/QSet>

#include <core/resource/layout_resource.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

namespace nx::vms::common {

class LayoutItemWatcher::Private: public QObject
{
    LayoutItemWatcher* const q;

public:
    explicit Private(LayoutItemWatcher* q): q(q) {}

    void handleItemAdded(const QnLayoutResourcePtr& layout, const LayoutItemData& item);
    void handleItemRemoved(const QnLayoutResourcePtr& layout, const LayoutItemData& item);
    void handleItemChanged(const QnLayoutResourcePtr& layout, const LayoutItemData& item,
        const LayoutItemData& oldItem);

public:
    struct ResourceData
    {
        QHash<QnLayoutResourcePtr /*layout*/, QSet<nx::Uuid> /*itemIds*/> itemsOnLayouts;
        QSet<QnLayoutResourcePtr> layouts; //< For convenience and cleaner interface.
    };

    QSet<QnLayoutResourcePtr> watchedLayouts;
    QHash<nx::Uuid /*resourceId*/, ResourceData> resourceData;
    QHash<QnLayoutResourcePtr, QSet<nx::Uuid>> layoutResources;
    mutable nx::Mutex mutex;
};

// ------------------------------------------------------------------------------------------------
// LayoutItemWatcher

LayoutItemWatcher::LayoutItemWatcher(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

LayoutItemWatcher::~LayoutItemWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

bool LayoutItemWatcher::addWatchedLayout(const QnLayoutResourcePtr& layout)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);

        if (d->watchedLayouts.contains(layout))
            return false;

        d->watchedLayouts.insert(layout);
    }

    NX_VERBOSE(this, "Added watched layout: %1", layout);

    for (auto item: layout->getItems())
        d->handleItemAdded(layout, item);

    connect(layout.get(), &QnLayoutResource::itemAdded,
        d.get(), &Private::handleItemAdded, Qt::DirectConnection);

    connect(layout.get(), &QnLayoutResource::itemRemoved,
        d.get(), &Private::handleItemRemoved, Qt::DirectConnection);

    connect(layout.get(), &QnLayoutResource::itemChanged,
        d.get(), &Private::handleItemChanged, Qt::DirectConnection);

    return true;
}

bool LayoutItemWatcher::removeWatchedLayout(const QnLayoutResourcePtr& layout)
{
    QSet<nx::Uuid> removedFromLayoutIds;
    QSet<nx::Uuid> removedFromTheWatcherIds;

    layout->disconnect(d.get());

    {
        NX_MUTEX_LOCKER lk(&d->mutex);

        if (!d->watchedLayouts.remove(layout))
            return false;

        // If `removeWatchedLayout` is called from layout's `update()`, current layout items
        // may be already updated, so we must use stored `layoutResources[layout]` data to
        // determine what resources are being removed.
        removedFromLayoutIds = d->layoutResources.take(layout);

        for (const auto& resourceId: removedFromLayoutIds)
        {
            auto& data = d->resourceData[resourceId];
            data.itemsOnLayouts.remove(layout);
            data.layouts.remove(layout);

            if (data.layouts.empty())
            {
                NX_ASSERT(data.itemsOnLayouts.empty());
                removedFromTheWatcherIds.insert(resourceId);
                d->resourceData.remove(resourceId);
            }
        }
    }

    NX_VERBOSE(this, "Removed watched layout: %1", layout);

    for (const auto resourceId: removedFromLayoutIds)
    {
        emit removedFromLayout(resourceId, layout);

        if (removedFromTheWatcherIds.contains(resourceId))
            emit resourceRemoved(resourceId);
    }

    return true;
}

QSet<QnLayoutResourcePtr> LayoutItemWatcher::watchedLayouts() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->watchedLayouts;
}

bool LayoutItemWatcher::isWatched(const QnLayoutResourcePtr& layout) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->watchedLayouts.contains(layout);
}

bool LayoutItemWatcher::hasResource(const nx::Uuid& resourceId) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->resourceData.contains(resourceId);
}

QSet<QnLayoutResourcePtr> LayoutItemWatcher::resourceLayouts(const nx::Uuid& resourceId) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->resourceData.value(resourceId).layouts;
}

// ------------------------------------------------------------------------------------------------
// LayoutItemWatcher::Private

void LayoutItemWatcher::Private::handleItemAdded(
    const QnLayoutResourcePtr& layout, const LayoutItemData& item)
{
    const auto resourceId = item.resource.id;
    if (resourceId.isNull())
        return;

    NX_VERBOSE(q, "Resource id %1 added to layout %2", resourceId, layout);

    bool addedToLayout = false;
    bool addedToFirstLayout = false;
    {
        NX_MUTEX_LOCKER lk(&mutex);
        auto& data = resourceData[resourceId];
        auto& items = data.itemsOnLayouts[layout];

        // If `addWatchedLayout()` was called inside `QnResource::update()` and this
        // `QnLayoutResource::itemAdded()` was called later from the same update,
        // the item is already added to the watcher and should be skipped here.
        if (items.contains(item.uuid))
            return;

        addedToFirstLayout = data.layouts.empty();
        addedToLayout = items.empty();
        data.layouts.insert(layout);
        items.insert(item.uuid);

        if (addedToLayout)
            layoutResources[layout].insert(resourceId);
    }

    if (addedToLayout)
        emit q->addedToLayout(resourceId, layout);

    if (addedToFirstLayout)
        emit q->resourceAdded(resourceId);
}

void LayoutItemWatcher::Private::handleItemRemoved(
    const QnLayoutResourcePtr& layout, const LayoutItemData& item)
{
    const auto resourceId = item.resource.id;
    if (resourceId.isNull())
        return;

    NX_VERBOSE(q, "Resource id %1 removed from layout %2", resourceId, layout);

    bool removedFromLayout = false;
    bool removedFromLastLayout = false;
    {
        NX_MUTEX_LOCKER lk(&mutex);

        // If `addWatchedLayout()` was called inside `QnResource::update()` and this
        // `QnLayoutResource::itemRemoved()` was called later from the same update,
        // the item is already not in the watcher and should be skipped here.
        if (!resourceData.value(resourceId).itemsOnLayouts.value(layout).contains(item.uuid))
            return;

        auto& data = resourceData[resourceId];
        auto& items = data.itemsOnLayouts[layout];

        items.remove(item.uuid);
        if (removedFromLayout = items.empty())
        {
            data.itemsOnLayouts.remove(layout);
            data.layouts.remove(layout);

            layoutResources[layout].remove(resourceId);

            if (removedFromLastLayout = data.layouts.empty())
            {
                NX_ASSERT(data.itemsOnLayouts.empty());
                resourceData.remove(resourceId);
            }
        }
    }

    if (removedFromLayout)
        emit q->removedFromLayout(resourceId, layout);

    if (removedFromLastLayout)
        emit q->resourceRemoved(resourceId);
}

void LayoutItemWatcher::Private::handleItemChanged(
    const QnLayoutResourcePtr& layout, const LayoutItemData& item, const LayoutItemData& oldItem)
{
    if (item.resource.id == oldItem.resource.id && NX_ASSERT(item.uuid == oldItem.uuid))
        return;

    handleItemRemoved(layout, oldItem);
    handleItemAdded(layout, item);
}

} // namespace nx::vms::common
