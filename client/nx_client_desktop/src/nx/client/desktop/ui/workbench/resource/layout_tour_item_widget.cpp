#include "layout_tour_item_widget.h"

#include <QtGui/QGuiApplication>

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <camera/camera_thumbnail_manager.h>

#include <core/resource/layout_resource.h>

#include <text/time_strings.h>

#include <ui/common/palette.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/standard/graphics_pixmap.h>
#include <nx/client/desktop/ui/graphics/painters/layout_preview_painter.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

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
        setAcceptedMouseButtons(0);
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
    m_previewPainter(new LayoutPreviewPainter(context->instance<QnCameraThumbnailManager>()))
{
    setOption(QnResourceWidget::InfoOverlaysForbidden);
    setOption(QnResourceWidget::WindowRotationForbidden);

    QnLayoutResourcePtr layout = resource().dynamicCast<QnLayoutResource>();
    if (!layout)
        layout = QnLayoutResource::createFromResource(resource());

    m_previewPainter->setLayout(layout);
    m_previewPainter->setFrameColor(Qt::black);                 //TODO: #GDM #3.1 customize
    m_previewPainter->setBackgroundColor(QColor("#222B2F"));    //TODO: #GDM #3.1 customize
    m_previewPainter->setFontColor(palette().color(QPalette::WindowText)); //TODO: #GDM #3.1 customize

    setAcceptDrops(true);

    initOverlay();
}

LayoutTourItemWidget::~LayoutTourItemWidget()
{
}

void LayoutTourItemWidget::initOverlay()
{
    auto font = this->font();
    font.setPixelSize(14);
    setFont(font);
    setPaletteColor(this, QPalette::WindowText, QColor("#a5b7c0")); //TODO: #GDM #3.1 customize

    auto titleFont(font);
    titleFont.setWeight(QFont::DemiBold);

    auto orderFont(font);
    orderFont.setPixelSize(18);

    auto icon = new GraphicsPixmap();
    icon->setAcceptedMouseButtons(0);

    auto updateIcon = [this, icon]
        {
            const auto pixmap = qnResIconCache->icon(resource())
                .pixmap(1024, 1024, QIcon::Normal, QIcon::On);
            icon->setPixmap(pixmap);
        };
    updateIcon();
    connect(resource(), &QnResource::parentIdChanged, this, updateIcon);

    auto title = new GraphicsLabel();
    title->setAcceptedMouseButtons(0);
    title->setPerformanceHint(GraphicsLabel::PixmapCaching);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignVCenter);

    auto updateTitle = [this, title]
        {
            title->setText(resource()->getName());
        };
    updateTitle();
    connect(resource(), &QnResource::nameChanged, this, updateTitle);

    auto closeButton = new QnImageButtonWidget();
    const auto closeButtonIcon = qnSkin->icon(lit("buttons/clear.png"));
    const auto closeButtonSize = QnSkin::maximumSize(closeButtonIcon);
    closeButton->setIcon(closeButtonIcon);
    closeButton->setFixedSize(closeButtonSize);
    connect(closeButton, &QnImageButtonWidget::clicked, this, &QnResourceWidget::close);

    auto headerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    headerLayout->addItem(icon);
    headerLayout->addItem(title);
    headerLayout->addStretch();
    headerLayout->addItem(closeButton);
    headerLayout->setSpacing(4.0);

    auto orderLabel = new GraphicsLabel();
    orderLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    orderLabel->setAcceptedMouseButtons(0);
    orderLabel->setAlignment(Qt::AlignVCenter);
    orderLabel->setFont(orderFont);
    auto updateOrder = [this, orderLabel](Qn::ItemDataRole role)
        {
            if (role != Qn::LayoutTourItemOrderRole)
                return;

            const int order = item()->data(role).toInt();
            orderLabel->setText(QString::number(order));
        };
    updateOrder(Qn::LayoutTourItemOrderRole);
    connect(item(), &QnWorkbenchItem::dataChanged, this, updateOrder);

    auto delayHintLabel = new GraphicsLabel(tr("Display for"));
    delayHintLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);
    delayHintLabel->setAcceptedMouseButtons(0);
    delayHintLabel->setAlignment(Qt::AlignVCenter);
    delayHintLabel->setFont(font);
    setPaletteColor(delayHintLabel, QPalette::WindowText, QColor("#53707f")); //TODO: #GDM #3.1 customize

    auto delayEdit = new QSpinBox();
    delayEdit->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));
    delayEdit->setMinimum(1);
    delayEdit->setMaximum(99);
    const auto delayMs = item()->data(Qn::LayoutTourItemDelayMsRole).toInt();
    delayEdit->setValue(delayMs / 1000);
    connect(delayEdit, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            item()->setData(Qn::LayoutTourItemDelayMsRole, value * 1000);
        });

    auto delayWidget = new QnMaskedProxyWidget();
    delayWidget->setWidget(delayEdit);

    auto footerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    footerLayout->addItem(orderLabel);
    footerLayout->addStretch();
    footerLayout->addItem(delayHintLabel);
    footerLayout->addItem(delayWidget);

    auto updateManualMode = [this, delayHintLabel, delayWidget]
        {
            const bool isManual = item()->layout()->data(Qn::LayoutTourIsManualRole).toBool();
            delayHintLabel->setVisible(!isManual);
            delayWidget->setVisible(!isManual);
        };

    connect(item()->layout(), &QnWorkbenchLayout::dataChanged, this,
        [updateManualMode](int role)
        {
            if (role == Qn::LayoutTourIsManualRole)
                updateManualMode();
        });
    updateManualMode();

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


