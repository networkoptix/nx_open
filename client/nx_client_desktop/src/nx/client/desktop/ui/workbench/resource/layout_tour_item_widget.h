#pragma once

#include <ui/graphics/items/resource/resource_widget.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class LayoutTourItemWidget: public QnResourceWidget
{
    Q_OBJECT
    using base_type = QnResourceWidget;
public:
    LayoutTourItemWidget(QnWorkbenchContext* context, QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);

private:
    void initOverlay();

private:
    QnLayoutResourcePtr m_layout;

};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
