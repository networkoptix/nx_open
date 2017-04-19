#pragma once

#include <QtCore/QObject>

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
    QnUuid currentTourId() const;
    bool isLayoutTourReviewMode() const;

    void connectToLayout(QnWorkbenchLayout* layout);
    void connectToItem(QnWorkbenchItem* item);

    void updateOrder();

private:
    QnDisconnectHelperPtr m_connections;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
