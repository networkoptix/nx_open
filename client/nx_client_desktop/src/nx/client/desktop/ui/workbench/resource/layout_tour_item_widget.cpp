#include "layout_tour_item_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/layout_resource.h>

#include <ui/common/palette.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <nx/client/desktop/ui/graphics/painters/layout_preview_painter.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

namespace {

class LayoutPreviewWidget: public GraphicsWidget
{
public:
    LayoutPreviewWidget(QSharedPointer<LayoutPreviewPainter> previewPainter):
        m_previewPainter(previewPainter)
    {
    }

    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option, QWidget* /*widget*/) override
    {
        if (auto previewPainter = m_previewPainter.lock())
            previewPainter->paint(painter, option->rect);
    }

private:
    QWeakPointer<LayoutPreviewPainter> m_previewPainter;
};

} // namespace

LayoutTourItemWidget::LayoutTourItemWidget(
    QnWorkbenchContext* context,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(context, item, parent),
    m_layout(base_type::resource().dynamicCast<QnLayoutResource>()),
    m_previewPainter(new LayoutPreviewPainter(context->instance<QnCameraThumbnailManager>()))
{
    NX_EXPECT(m_layout, "LayoutTourItemWidget was created with a non-layout resource.");
    m_previewPainter->setLayout(m_layout);

    setAcceptDrops(true);

    initOverlay();
}

LayoutTourItemWidget::~LayoutTourItemWidget()
{
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

    auto contentWidget = new LayoutPreviewWidget(m_previewPainter);

    auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(10.0, 5.0, 5.0, 10.0);
    layout->addItem(contentWidget);
    layout->addItem(footerWidget);
    layout->setStretchFactor(contentWidget, 1);

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


