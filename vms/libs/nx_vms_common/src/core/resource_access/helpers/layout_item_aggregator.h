// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <utils/common/counter_hash.h>

#include <nx/utils/uuid.h>


/**
 * This class watches set of given layouts, combining all items on them.
 * It will notify if any item will be added to this layout set for the first time and if any item
 * will be removed from the last layout it belongs to.
 * NOTE: class does not depend on resource pool, so even deleted layouts must be removed manually.
 * WARNING: this class is not thread-safe.
 */
class NX_VMS_COMMON_API QnLayoutItemAggregator: public QObject
{
    Q_OBJECT

    using base_type = QObject;
public:

    QnLayoutItemAggregator(QObject* parent = nullptr);
    virtual ~QnLayoutItemAggregator();

    /** Start watching layout. Return false if layout was already watched.  */
    bool addWatchedLayout(const QnLayoutResourcePtr& layout);

    /** Stops watching layout. Returns false if layout was not watched. */
    bool removeWatchedLayout(const QnLayoutResourcePtr& layout);

    QSet<QnLayoutResourcePtr> watchedLayouts() const;

    bool hasLayout(const QnLayoutResourcePtr& layout) const;
    bool hasItem(const QnUuid& id) const;

signals:
    void itemAdded(const QnUuid& resourceId);
    void itemRemoved(const QnUuid& resourceId);

private:
    void handleItemAdded(const QnLayoutItemData& item);
    void handleItemRemoved(const QnLayoutItemData& item);

private:
    QSet<QnLayoutResourcePtr> m_watchedLayouts;
    QnCounterHash<QnUuid> m_items;
};
