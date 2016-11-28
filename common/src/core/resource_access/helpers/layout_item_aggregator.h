#pragma once

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>
#include <utils/common/counter_hash.h>

#include <nx/utils/uuid.h>


/**
 * This class watches set of given layouts, combining all items on them.
 * It will notify if any item will be added to this layout set for the first time and if any item
 * will be removed from the last layout it belongs to.
 * NOTE: class does not depend on resource pool, so even deleted layouts must be removed manually.
 * WARNING: this class is not thread-safe.
 */
class QnLayoutItemAggregator: public Connective<QObject>
{
    Q_OBJECT

    using base_type = Connective<QObject>;
public:

    QnLayoutItemAggregator(QObject* parent = nullptr);
    virtual ~QnLayoutItemAggregator();

    void addWatchedLayout(const QnLayoutResourcePtr& layout);
    void removeWatchedLayout(const QnLayoutResourcePtr& layout);

    using key_iterator = QnCounterHash<QnLayoutResourcePtr>::key_iterator;
    key_iterator layoutBegin() const;
    key_iterator layoutEnd() const;

    bool hasLayout(const QnLayoutResourcePtr& layout) const;
    bool hasItem(const QnUuid& id) const;

signals:
    void itemAdded(const QnUuid& resourceId);
    void itemRemoved(const QnUuid& resourceId);

private:
    void handleItemAdded(const QnLayoutItemData& item);
    void handleItemRemoved(const QnLayoutItemData& item);

private:
    QnCounterHash<QnLayoutResourcePtr> m_watchedLayouts;
    QnCounterHash<QnUuid> m_items;
};
