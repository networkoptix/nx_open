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

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutTourReviewController: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    LayoutTourReviewController(QObject* parent = nullptr);
    virtual ~LayoutTourReviewController() override;

private:
    void reviewLayoutTour(const ec2::ApiLayoutTourData& tour);

    QnUuid currentTourId() const;
    bool isLayoutTourReviewMode() const;

    void connectToLayout(QnWorkbenchLayout* layout);
    void connectToItem(QnWorkbenchItem* item);

    void updateOrder();
    void updateButtons(const QnLayoutResourcePtr& layout);

    void addItemToReviewLayout(
        const QnLayoutResourcePtr& layout,
        const ec2::ApiLayoutTourItemData& item,
        const QPointF& position = QPointF());

    /** Calculate items from the review layout. */
    bool fillTourItems(ec2::ApiLayoutTourItemDataList* items);

private:
    QnDisconnectHelperPtr m_connections;
    QHash<QnUuid, QnLayoutResourcePtr> m_reviewLayouts;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
