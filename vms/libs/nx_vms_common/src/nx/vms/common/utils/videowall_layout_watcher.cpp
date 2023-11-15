// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_layout_watcher.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
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

    void handleVideowallMatrixAdded(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallMatrix& matrix);

    void handleVideowallMatrixRemoved(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallMatrix& matrix);

    void handleVideowallMatrixChanged(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallMatrix& matrix, const QnVideoWallMatrix& oldMatrix);

private:
    void addVideowallUnsafe(const QnVideoWallResourcePtr& videowall, nx::MutexLocker&);

    void handleVideowallLayoutsChanged(const QnVideoWallResourcePtr& videowall,
        QSet<QnUuid> addedLayoutIds, QSet<QnUuid> removedLayoutIds);

    QSet<QnUuid> itemIds(const QnVideoWallResourcePtr& videowall) const;

public:
    struct VideowallData
    {
        QnCounterHash<QnUuid> layouts; //< All, active & inactive.
        QSet<QnUuid> itemIds; //< Saved item ids, required for proper matrix item tracking.
    };

    const QPointer<QnResourcePool> resourcePool;
    QHash<QnVideoWallResourcePtr, VideowallData> videowallData;
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
    return d->videowallData.value(videowall).layouts;
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

    NX_MUTEX_LOCKER lk(&mutex);
    const auto videowalls = resourcePool->getResources<QnVideoWallResource>();

    for (const auto& videowall: videowalls)
        addVideowallUnsafe(videowall, lk);
}

void VideowallLayoutWatcher::Private::handleVideowallAdded(
    const QnVideoWallResourcePtr& videowall)
{
    NX_MUTEX_LOCKER lk(&mutex);
    addVideowallUnsafe(videowall, lk);
    lk.unlock();
    emit q->videowallAdded(videowall);
}

void VideowallLayoutWatcher::Private::handleVideowallRemoved(
    const QnVideoWallResourcePtr& videowall)
{
    NX_VERBOSE(q, "Videowall %1 is removed from the pool, it had %2", videowall,
        layoutsToLogString(videowallData.value(videowall).layouts.keys()));

    videowall->disconnect(this);

    NX_MUTEX_LOCKER lk(&mutex);

    const auto data = videowallData.take(videowall);

    for (const auto& layout: nx::utils::keyRange(data.layouts))
        allVideowallLayouts.remove(layout);

    lk.unlock();
    emit q->videowallRemoved(videowall);
}

void VideowallLayoutWatcher::Private::handleVideowallItemAdded(
    const QnVideoWallResourcePtr& videowall, const QnVideoWallItem& item)
{
    handleVideowallItemChanged(videowall, item, {});

    NX_MUTEX_LOCKER lk(&mutex);
    videowallData[videowall].itemIds.insert(item.uuid);
}

void VideowallLayoutWatcher::Private::handleVideowallItemRemoved(
    const QnVideoWallResourcePtr& videowall, const QnVideoWallItem& item)
{
    handleVideowallItemChanged(videowall, {}, item);

    NX_MUTEX_LOCKER lk(&mutex);
    videowallData[videowall].itemIds.remove(item.uuid);
}

void VideowallLayoutWatcher::Private::handleVideowallItemChanged(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallItem& newItem,
    const QnVideoWallItem& oldItem)
{
    if (newItem.layout == oldItem.layout)
        return;

    const auto addedLayoutIds = newItem.layout.isNull()
        ? QSet<QnUuid>()
        : QSet<QnUuid>({newItem.layout});

    const auto removedLayoutIds = oldItem.layout.isNull()
        ? QSet<QnUuid>()
        : QSet<QnUuid>({oldItem.layout});

    handleVideowallLayoutsChanged(videowall, addedLayoutIds, removedLayoutIds);
}

void VideowallLayoutWatcher::Private::handleVideowallMatrixAdded(
    const QnVideoWallResourcePtr& videowall, const QnVideoWallMatrix& matrix)
{
    handleVideowallMatrixChanged(videowall, matrix, {});
}

void VideowallLayoutWatcher::Private::handleVideowallMatrixRemoved(
    const QnVideoWallResourcePtr& videowall, const QnVideoWallMatrix& matrix)
{
    handleVideowallMatrixChanged(videowall, {}, matrix);
}

void VideowallLayoutWatcher::Private::handleVideowallMatrixChanged(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallMatrix& matrix,
    const QnVideoWallMatrix& oldMatrix)
{
    if (!NX_ASSERT(videowall))
        return;

    QSet<QnUuid> addedLayoutIds;
    QSet<QnUuid> removedLayoutIds;

    const auto itemIds = this->itemIds(videowall);
    for (const auto& itemId: itemIds)
    {
        if (const auto newLayoutId = matrix.layoutByItem.value(itemId); !newLayoutId.isNull())
            addedLayoutIds.insert(newLayoutId);

        if (const auto oldLayoutId = oldMatrix.layoutByItem.value(itemId); !oldLayoutId.isNull())
            removedLayoutIds.insert(oldLayoutId);
    }

    handleVideowallLayoutsChanged(videowall, addedLayoutIds, removedLayoutIds);
}

void VideowallLayoutWatcher::Private::handleVideowallLayoutsChanged(
    const QnVideoWallResourcePtr& videowall,
    QSet<QnUuid> addedLayoutIds,
    QSet<QnUuid> removedLayoutIds)
{
    const auto unchanged = addedLayoutIds & removedLayoutIds;
    addedLayoutIds -= unchanged;
    removedLayoutIds -= unchanged;

    if (addedLayoutIds.empty() && removedLayoutIds.empty())
        return;

    NX_MUTEX_LOCKER lk(&mutex);
    auto& data = videowallData[videowall];

    QSet<QnUuid> actuallyRemoved;
    QSet<QnUuid> actuallyAdded;

    for (const auto removedLayoutId: removedLayoutIds)
    {
        allVideowallLayouts.remove(removedLayoutId);

        if (data.layouts.remove(removedLayoutId))
            actuallyRemoved.insert(removedLayoutId);
    }

    for (const auto addedLayoutId: addedLayoutIds)
    {
        allVideowallLayouts.insert(addedLayoutId);

        if (data.layouts.insert(addedLayoutId))
            actuallyAdded.insert(addedLayoutId);
    }

    lk.unlock();

    if (!actuallyRemoved.empty())
    {
        NX_VERBOSE(q, "Videowall %1 layouts %2 are removed", videowall,
            nx::containerString(actuallyRemoved));
        emit q->videowallLayoutsRemoved(
            videowall, {actuallyRemoved.cbegin(), actuallyRemoved.cend()});
    }

    if (!actuallyAdded.empty())
    {
        NX_VERBOSE(q, "Videowall %1 layouts %2 are added", videowall,
            nx::containerString(actuallyAdded));
        emit q->videowallLayoutsAdded(
            videowall, {actuallyAdded.cbegin(), actuallyAdded.cend()});
    }
}

void VideowallLayoutWatcher::Private::addVideowallUnsafe(
    const QnVideoWallResourcePtr& videowall, nx::MutexLocker&)
{
    if (!NX_ASSERT(!videowallData.contains(videowall), "Videowall is already watched"))
        return;

    connect(videowall.get(), &QnVideoWallResource::itemAdded,
        this, &Private::handleVideowallItemAdded, Qt::DirectConnection);

    connect(videowall.get(), &QnVideoWallResource::itemRemoved,
        this, &Private::handleVideowallItemRemoved, Qt::DirectConnection);

    connect(videowall.get(), &QnVideoWallResource::itemChanged,
        this, &Private::handleVideowallItemChanged, Qt::DirectConnection);

    connect(videowall.get(), &QnVideoWallResource::matrixAdded,
        this, &Private::handleVideowallMatrixAdded, Qt::DirectConnection);

    connect(videowall.get(), &QnVideoWallResource::matrixRemoved,
        this, &Private::handleVideowallMatrixRemoved, Qt::DirectConnection);

    connect(videowall.get(), &QnVideoWallResource::matrixChanged,
        this, &Private::handleVideowallMatrixChanged, Qt::DirectConnection);

    auto& data = videowallData[videowall];
    for (const auto& item: videowall->items()->getItems())
    {
        if (item.layout.isNull())
            continue;

        data.layouts.insert(item.layout);
        data.itemIds.insert(item.uuid);
        allVideowallLayouts.insert(item.layout);
    }

    NX_VERBOSE(q, "Videowall %1 is added to the pool, it has %2", videowall,
        layoutsToLogString(data.layouts.keys()));
}

QSet<QnUuid> VideowallLayoutWatcher::Private::itemIds(const QnVideoWallResourcePtr& videowall) const
{
    NX_MUTEX_LOCKER lk(&mutex);
    return videowallData.value(videowall).itemIds;
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
