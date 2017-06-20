#pragma once

#include <QtCore/QObject>

#include <nx_ec/data/api_fwd.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

#include <nx/utils/disconnect_helper.h>
#include <nx/utils/uuid.h>

class QnWorkbenchLayout;
class QnWorkbenchItem;
class QGraphicsWidget;
class QnPendingOperation;

namespace nx {
namespace client {
namespace desktop {
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
    void handleTourChanged(const ec2::ApiLayoutTourData& tour);
    void handleTourRemoved(const QnUuid& tourId);

    /** Handle current layout changes to update the tour review. */
    void startListeningLayout();
    /** Stop handling current layout changes. */
    void stopListeningLayout();

    void reviewLayoutTour(const ec2::ApiLayoutTourData& tour);

    QnUuid currentTourId() const;
    bool isLayoutTourReviewMode() const;

    void connectToLayout(QnWorkbenchLayout* layout);

    void updateOrder();
    void updateButtons(const QnLayoutResourcePtr& layout);
    void updatePlaceholders();
    void updateItemsLayout();

    void resetReviewLayout(const QnLayoutResourcePtr& layout,
        const ec2::ApiLayoutTourItemDataList& items);

    void addItemToReviewLayout(
        const QnLayoutResourcePtr& layout,
        const ec2::ApiLayoutTourItemData& item,
        const QPointF& position,
        bool pinItem);
    void addResourcesToReviewLayout(
        const QnLayoutResourcePtr& layout,
        const QnResourceList& resources,
        const QPointF& position);

    /** Calculate items from the review layout. */
    bool fillTourItems(ec2::ApiLayoutTourItemDataList* items);

    void handleItemDataChanged(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

// Actions handlers
private:
    void at_reviewLayoutTourAction_triggered();
    void at_dropResourcesAction_triggered();
    void at_startCurrentLayoutTourAction_triggered();
    void at_saveCurrentLayoutTourAction_triggered();
    void at_removeCurrentLayoutTourAction_triggered();

private:
    QnDisconnectHelperPtr m_connections;
    QHash<QnUuid, QnLayoutResourcePtr> m_reviewLayouts;
    QHash<QPoint, QSharedPointer<LayoutTourDropPlaceholder> > m_dropPlaceholders;
    QSet<QnUuid> m_saveToursQueue;
    QnPendingOperation* m_saveToursOperation = nullptr;
    QnPendingOperation* m_updateItemsLayoutOperation = nullptr;
    bool m_updating = false;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
