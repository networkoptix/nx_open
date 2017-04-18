#include "layout_tour_item_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <core/resource/layout_resource.h>

#include <ui/common/palette.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>

#include <ui/workbench/workbench_item.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

LayoutTourItemWidget::LayoutTourItemWidget(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(context, item, parent),
    m_layout(base_type::resource().dynamicCast<QnLayoutResource>())
{
    NX_EXPECT(m_layout, "LayoutTourItemWidget was created with a non-layout resource.");

    setAcceptDrops(true);

    initOverlay();
}

void LayoutTourItemWidget::initOverlay()
{
    auto delayLabel = new GraphicsLabel();
    delayLabel->setAcceptedMouseButtons(0);
    delayLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    delayLabel->setText(QString::number(item()->data(Qn::LayoutTourItemDelayMsRole).toInt()));

    auto footerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    footerLayout->addStretch();
    footerLayout->addItem(delayLabel);

    auto footerWidget = new GraphicsWidget();
    footerWidget->setAutoFillBackground(true);
    setPaletteColor(footerWidget, QPalette::Window, Qt::darkBlue);
    footerWidget->setLayout(footerLayout);

    auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(10.0, 5.0, 5.0, 10.0);
    layout->addItem(footerWidget);

    auto overlayWidget = new QnViewportBoundWidget(this);
    overlayWidget->setLayout(layout);
    overlayWidget->setAcceptedMouseButtons(0);
    addOverlayWidget(overlayWidget, Visible);
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx


