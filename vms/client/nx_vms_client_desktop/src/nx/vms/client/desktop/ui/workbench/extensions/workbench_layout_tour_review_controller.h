#pragma once

#include <QtCore/QObject>

#include <nx_ec/data/api_fwd.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>

class QnWorkbenchLayout;
class QnWorkbenchItem;
class QGraphicsWidget;

namespace nx::utils { class PendingOperation; }

namespace nx::vms::client::desktop {
namespace ui {

class LayoutTourDropPlaceholder;

namespace workbench {

class LayoutTourReviewController: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    LayoutTourReviewController(QObject* parent = nullptr);
    virtual ~LayoutTourReviewController() override;

private:
    void handleTourChanged(const nx::vms::api::LayoutTourData& tour);
    void handleTourRemoved(const QnUuid& tourId);

    /** Handle current layout changes to update the tour review. */
    void startListeningLayout();
    /** Stop handling current layout changes. */
    void stopListeningLayout();

    void reviewLayoutTour(const nx::vms::api::LayoutTourData& tour);

    QnUuid currentTourId() const;
    bool isLayoutTourReviewMode() const;

    void connectToLayout(QnWorkbenchLayout* layout);

    void updateOrder();
    void updateButtons(const QnLayoutResourcePtr& layout);
    void updatePlaceholders();
    void updateItemsLayout();

    void resetReviewLayout(const QnLayoutResourcePtr& layout,
        const nx::vms::api::LayoutTourItemDataList& items);

    void addItemToReviewLayout(
        const QnLayoutResourcePtr& layout,
        const nx::vms::api::LayoutTourItemData& item,
        const QPointF& position,
        bool pinItem);
    void addResourcesToReviewLayout(
        const QnLayoutResourcePtr& layout,
        const QnResourceList& resources,
        const QPointF& position);

    /** Calculate items from the review layout. */
    bool fillTourItems(nx::vms::api::LayoutTourItemDataList* items);

    void handleItemDataChanged(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

// Actions handlers
private:
    void at_reviewLayoutTourAction_triggered();
    void at_dropResourcesAction_triggered();
    void at_startCurrentLayoutTourAction_triggered();
    void at_saveCurrentLayoutTourAction_triggered();
    void at_removeCurrentLayoutTourAction_triggered();

private:
    nx::utils::ScopedConnections m_connections;
    QHash<QnUuid, QnLayoutResourcePtr> m_reviewLayouts;
    QHash<QPoint, QSharedPointer<LayoutTourDropPlaceholder> > m_dropPlaceholders;
    QSet<QnUuid> m_saveToursQueue;
    nx::utils::PendingOperation* m_saveToursOperation = nullptr;
    nx::utils::PendingOperation* m_updateItemsLayoutOperation = nullptr;
    bool m_updating = false;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop
