// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_layout_watcher.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

class QnResourcePool;

namespace nx::vms::common {

namespace {

QString layoutsToLogString(const auto& layouts)
{
    if (layouts.empty())
        return "no layouts";

    return NX_FMT("%1 layout(s): %2", layouts.size(), layouts);
};

} // namespace

class VideowallLayoutWatcher::Private: public QObject
{
    VideowallLayoutWatcher* const q;

public:
    explicit Private(VideowallLayoutWatcher* q, QnResourcePool* resourcePool);

    void handleVideowallAdded(const QnVideoWallResourcePtr& videowall);
    void handleVideowallRemoved(const QnVideoWallResourcePtr& videowall);

    void handleVideowallItemAdded(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& item);

    void handleVideowallItemRemoved(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& item);

    void handleVideowallItemChanged(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& newItem, const QnVideoWallItem& oldItem);

private:
    void addVideowall(const QnVideoWallResourcePtr& videowall);

public:
    const QPointer<QnResourcePool> resourcePool;
    QHash<QnVideoWallResourcePtr, QnCounterHash<QnUuid>> videowallLayouts;
    QnCounterHash<QnUuid> allVideowallLayouts;
    mutable nx::Mutex mutex;

public:
    class InstanceRegistry: public QObject
    {
    public:
        std::shared_ptr<VideowallLayoutWatcher> instance(QnResourcePool* resourcePool);

    private:
        QHash<QnResourcePool*, std::weak_ptr<VideowallLayoutWatcher>> m_instances;
        nx::Mutex m_mutex;
    };
};

// -----------------------------------------------------------------------------------------------
// VideowallLayoutWatcher

VideowallLayoutWatcher::VideowallLayoutWatcher(QnResourcePool* resourcePool):
    d(new Private(this, resourcePool))
{
}

VideowallLayoutWatcher::~VideowallLayoutWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

std::shared_ptr<VideowallLayoutWatcher> VideowallLayoutWatcher::instance(
    QnResourcePool* resourcePool)
{
    if (!NX_ASSERT(resourcePool))
        return {};

    static Private::InstanceRegistry registry;
    return registry.instance(resourcePool);
}

QnVideoWallResourcePtr VideowallLayoutWatcher::layoutVideowall(
    const QnLayoutResourcePtr& layout) const
{
    if (!NX_ASSERT(layout) || !NX_ASSERT(layout->resourcePool() == d->resourcePool))
        return {};

    const auto videowall = layout->getParentResource().objectCast<QnVideoWallResource>();

    return videowall && videowallLayouts(videowall).contains(layout->getId())
        ? videowall
        : QnVideoWallResourcePtr();
}

QnCounterHash<QnUuid> VideowallLayoutWatcher::videowallLayouts(
    const QnVideoWallResourcePtr& videowall) const
{
    if (!NX_ASSERT(videowall)
        || !videowall->resourcePool()
        || !NX_ASSERT(videowall->resourcePool() == d->resourcePool))
    {
        return {};
    }

    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->videowallLayouts.value(videowall);
}

bool VideowallLayoutWatcher::isVideowallItem(const QnUuid& layoutId) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->allVideowallLayouts.contains(layoutId);
}

// -----------------------------------------------------------------------------------------------
// VideowallLayoutWatcher::Private

VideowallLayoutWatcher::Private::Private(
    VideowallLayoutWatcher* q,
    QnResourcePool* resourcePool)
    :
    q(q),
    resourcePool(resourcePool)
{
    if (!NX_ASSERT(resourcePool))
        return;

    connect(resourcePool, &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (const auto videowall = resource.objectCast<QnVideoWallResource>())
                    handleVideowallAdded(videowall);
            }
        }, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (const auto videowall = resource.objectCast<QnVideoWallResource>())
                    handleVideowallRemoved(videowall);
            }
        }, Qt::DirectConnection);

    const auto videowalls = resourcePool->getResources<QnVideoWallResource>();
    for (const auto& videowall: videowalls)
        addVideowall(videowall);
}

void VideowallLayoutWatcher::Private::handleVideowallAdded(
    const QnVideoWallResourcePtr& videowall)
{
    NX_MUTEX_LOCKER lk(&mutex);
    addVideowall(videowall);
    lk.unlock();
    emit q->videowallAdded(videowall);
}

void VideowallLayoutWatcher::Private::handleVideowallRemoved(
    const QnVideoWallResourcePtr& videowall)
{
    NX_VERBOSE(q, "Videowall %1 is removed from the pool, it had %2", videowall,
        layoutsToLogString(videowallLayouts.value(videowall).keys()));

    videowall->disconnect(this);

    NX_MUTEX_LOCKER lk(&mutex);
    videowallLayouts.remove(videowall);

    for (const auto& item: videowall->items()->getItems())
    {
        if (!item.layout.isNull())
            allVideowallLayouts.remove(item.layout);
    }

    lk.unlock();
    emit q->videowallRemoved(videowall);
}

void VideowallLayoutWatcher::Private::handleVideowallItemAdded(
    const QnVideoWallResourcePtr& videowall, const QnVideoWallItem& item)
{
    handleVideowallItemChanged(videowall, item, {});
}

void VideowallLayoutWatcher::Private::handleVideowallItemRemoved(
    const QnVideoWallResourcePtr& videowall, const QnVideoWallItem& item)
{
    handleVideowallItemChanged(videowall, {}, item);
}

void VideowallLayoutWatcher::Private::handleVideowallItemChanged(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallItem& newItem,
    const QnVideoWallItem& oldItem)
{
    if (oldItem.layout == newItem.layout)
        return;

    NX_MUTEX_LOCKER lk(&mutex);

    const bool hadOldLayout = !oldItem.layout.isNull();
    const bool hasNewLayout = !newItem.layout.isNull();

    auto& layouts = videowallLayouts[videowall];
    const bool removedLayout = hadOldLayout && layouts.remove(oldItem.layout);
    const bool addedLayout = hasNewLayout && layouts.insert(newItem.layout);

    if (hadOldLayout)
        allVideowallLayouts.remove(oldItem.layout);

    if (hasNewLayout)
        allVideowallLayouts.insert(newItem.layout);

    lk.unlock();

    if (removedLayout)
    {
        NX_VERBOSE(q, "Videowall %1 layout %2 is removed", videowall, oldItem.layout);
        emit q->videowallLayoutRemoved(videowall, oldItem.layout);
    }

    if (addedLayout)
    {
        NX_VERBOSE(q, "Videowall %1 layout %2 is added", videowall, newItem.layout);
        emit q->videowallLayoutAdded(videowall, newItem.layout);
    }
}

void VideowallLayoutWatcher::Private::addVideowall(const QnVideoWallResourcePtr& videowall)
{
    if (!NX_ASSERT(!videowallLayouts.contains(videowall), "Videowall is already watched"))
        return;

    connect(videowall.get(), &QnVideoWallResource::itemAdded,
        this, &Private::handleVideowallItemAdded, Qt::DirectConnection);

    connect(videowall.get(), &QnVideoWallResource::itemRemoved,
        this, &Private::handleVideowallItemRemoved, Qt::DirectConnection);

    connect(videowall.get(), &QnVideoWallResource::itemChanged,
        this, &Private::handleVideowallItemChanged, Qt::DirectConnection);

    auto& layouts = videowallLayouts[videowall];
    for (const auto& item: videowall->items()->getItems())
    {
        if (item.layout.isNull())
            continue;

        layouts.insert(item.layout);
        allVideowallLayouts.insert(item.layout);
    }

    NX_VERBOSE(q, "Videowall %1 is added to the pool, it has %2", videowall,
        layoutsToLogString(layouts.keys()));
}

// -----------------------------------------------------------------------------------------------
// VideowallLayoutWatcher::Private::InstanceRegistry

std::shared_ptr<VideowallLayoutWatcher>
    VideowallLayoutWatcher::Private::InstanceRegistry::instance(QnResourcePool* resourcePool)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    auto result = m_instances.value(resourcePool).lock();
    if (!result)
    {
        connect(resourcePool, &QObject::destroyed, this,
            [this]()
            {
                NX_MUTEX_LOCKER lk(&m_mutex);
                m_instances.remove(static_cast<QnResourcePool*>(sender()));
            });

        result.reset(new VideowallLayoutWatcher(resourcePool));
        m_instances.emplace(resourcePool, result);
    }

    return result;
}

} // namespace nx::vms::common
