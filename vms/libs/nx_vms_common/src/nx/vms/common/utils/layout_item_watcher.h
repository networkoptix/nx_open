// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <utils/common/counter_hash.h>

namespace nx::vms::common {

// This class is intended to supersede QnLayoutItemAggregator.

/**
 * This class watches set of given layouts, combining all items on them.
 * It tracks which resources are present on which layouts.
 *
 * It will notify when a resource is added to a layout for the first time and when a resource
 * is removed from the layout as a last item of that resource.
 *
 * It will also notify when a resource is added to this layout set for the first time and when
 * a resource is removed from the last layout it belongs to.
 *
 * NOTE: class does not depend on resource pool, so even deleted layouts must be removed manually.
 */
class NX_VMS_COMMON_API LayoutItemWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LayoutItemWatcher(QObject* parent = nullptr);
    virtual ~LayoutItemWatcher() override;

    /** Start watching layout. Return false if layout was already watched. */
    bool addWatchedLayout(const QnLayoutResourcePtr& layout);

    /** Stops watching layout. Returns false if layout was not watched. */
    bool removeWatchedLayout(const QnLayoutResourcePtr& layout);

    QSet<QnLayoutResourcePtr> watchedLayouts() const;
    bool isWatched(const QnLayoutResourcePtr& layout) const;

    bool hasResource(const QnUuid& resourceId) const;
    QnCounterHash<QnLayoutResourcePtr> resourceLayouts(const QnUuid& resourceId) const;

signals:
    void addedToLayout(const QnUuid& resourceId, const QnLayoutResourcePtr& layout);
    void removedFromLayout(const QnUuid& resourceId, const QnLayoutResourcePtr& layout);
    void resourceAdded(const QnUuid& resourceId);
    void resourceRemoved(const QnUuid& resourceId);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
