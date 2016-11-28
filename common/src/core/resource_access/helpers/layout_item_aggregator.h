#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

/**
 * This class watches set of given layouts, combining all items on them.
 * It will notify if any item will be added to this layout set for the first time and if any item
 * will be removed from the last layout it belongs to.
 * NOTE: class does not depend on resource pool, so even deleted layouts must be removed manually.
 */
class QnLayoutItemAggregator: public QObject
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnLayoutItemAggregator(QObject* parent = nullptr);
    virtual ~QnLayoutItemAggregator();

    QSet<QnLayoutResourcePtr> watchedLayouts() const;
};
