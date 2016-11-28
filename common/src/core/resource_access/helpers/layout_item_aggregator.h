#pragma once

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

#include <nx/utils/uuid.h>

/**
 * This class watches set of given layouts, combining all items on them.
 * It will notify if any item will be added to this layout set for the first time and if any item
 * will be removed from the last layout it belongs to.
 * NOTE: class does not depend on resource pool, so even deleted layouts must be removed manually.
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

    QnLayoutResourceSet watchedLayouts() const;

    bool hasItem(const QnUuid& id) const;

private:
    QnLayoutResourceSet m_watchedLayouts;
    QSet<QnUuid> m_items;
};
