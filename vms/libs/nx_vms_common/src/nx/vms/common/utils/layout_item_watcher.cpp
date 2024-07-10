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
    QSet<QnLayoutResourcePtr> watchedLayouts;
    QHash<nx::Uuid, QnCounterHash<QnLayoutResourcePtr>> itemLayouts;
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
    {
        NX_MUTEX_LOCKER lk(&d->mutex);

        if (!d->watchedLayouts.remove(layout))
            return false;
    }

    layout->disconnect(d.get());

    NX_VERBOSE(this, "Removed watched layout: %1", layout);

    for (auto item: layout->getItems())
        d->handleItemRemoved(layout, item);

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
    return d->itemLayouts.contains(resourceId);
}

QnCounterHash<QnLayoutResourcePtr> LayoutItemWatcher::resourceLayouts(
    const nx::Uuid& resourceId) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->itemLayouts.value(resourceId);
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
        addedToFirstLayout = !itemLayouts.contains(resourceId);
        addedToLayout = itemLayouts[resourceId].insert(layout);
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
        if (!NX_ASSERT(itemLayouts.contains(resourceId)))
            return;
        auto& layouts = itemLayouts[resourceId];
        removedFromLayout = layouts.remove(layout);
        removedFromLastLayout = layouts.empty();
        if (removedFromLastLayout)
            itemLayouts.remove(resourceId);
    }

    if (removedFromLayout)
        emit q->removedFromLayout(resourceId, layout);

    if (removedFromLastLayout)
        emit q->resourceRemoved(resourceId);
}

void LayoutItemWatcher::Private::handleItemChanged(
    const QnLayoutResourcePtr& layout, const LayoutItemData& item, const LayoutItemData& oldItem)
{
    if (item.resource.id == oldItem.resource.id)
        return;

    handleItemRemoved(layout, oldItem);
    handleItemAdded(layout, item);
}

} // namespace nx::vms::common
