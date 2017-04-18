#include "layout_tour_item_widget.h"

#include <QtGui/QGuiApplication>

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/layout_resource.h>

#include <ui/common/palette.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/standard/graphics_pixmap.h>
#include <nx/client/desktop/ui/graphics/painters/layout_preview_painter.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
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
    setOption(QnResourceWidget::InfoOverlaysForbidden);
    setOption(QnResourceWidget::WindowRotationForbidden);

    NX_EXPECT(m_layout, "LayoutTourItemWidget was created with a non-layout resource.");
    m_previewPainter->setLayout(m_layout);
    m_previewPainter->setFrameColor(Qt::black);
    m_previewPainter->setBackgroundColor(QColor("#222B2F"));

    setAcceptDrops(true);

    initOverlay();
}

LayoutTourItemWidget::~LayoutTourItemWidget()
{
}

void LayoutTourItemWidget::initOverlay()
{
    auto icon = new GraphicsPixmap();

    auto updateIcon = [this, icon]
        {
            const auto pixmap =  qnResIconCache->icon(m_layout)
                .pixmap(1024, 1024, QIcon::Normal, QIcon::On);
            icon->setPixmap(pixmap);
        };
    updateIcon();
    connect(m_layout, &QnResource::parentIdChanged, this, updateIcon);

    auto title = new GraphicsLabel();
    title->setAcceptedMouseButtons(0);
    title->setPerformanceHint(GraphicsLabel::PixmapCaching);
    auto font = title->font();
    font.setWeight(QFont::DemiBold);
    title->setFont(font);

    auto updateTitle = [this, title]
        {
            title->setText(m_layout->getName());
        };
    updateTitle();
    connect(m_layout, &QnResource::nameChanged, this, updateTitle);

    auto closeButton = new QnImageButtonWidget();
    const auto closeButtonIcon = qnSkin->icon(lit("events/notification_close.png"));
    const auto closeButtonSize = QnSkin::maximumSize(closeButtonIcon);
    closeButton->setIcon(closeButtonIcon);
    closeButton->setFixedSize(closeButtonSize);

    auto headerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    headerLayout->addItem(icon);
    headerLayout->addItem(title);
    headerLayout->addStretch();
    headerLayout->addItem(closeButton);

    auto delayLabel = new GraphicsLabel();
    delayLabel->setAcceptedMouseButtons(0);
    delayLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    delayLabel->setText(QString::number(item()->data(Qn::LayoutTourItemDelayMsRole).toInt()));

    auto footerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    footerLayout->addStretch();
    footerLayout->addItem(delayLabel);

    auto contentWidget = new LayoutPreviewWidget(m_previewPainter);

    auto layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(10.0, 5.0, 5.0, 10.0);
    layout->addItem(headerLayout);
    layout->addItem(contentWidget);
    layout->addItem(footerLayout);
    layout->setStretchFactor(contentWidget, 1000);

    auto overlayWidget = new QnViewportBoundWidget(this);
    overlayWidget->setLayout(layout);
    overlayWidget->setAcceptedMouseButtons(0);
    overlayWidget->setAutoFillBackground(true);
    setPaletteColor(overlayWidget, QPalette::Window, qApp->palette().color(QPalette::Window));
    addOverlayWidget(overlayWidget, Visible);
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx


