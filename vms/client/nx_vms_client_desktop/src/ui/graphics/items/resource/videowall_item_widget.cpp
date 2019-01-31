#include "videowall_item_widget.h"

#include <QtCore/QMimeData>

#include <QtGui/QDrag>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <common/common_globals.h>

#include <nx/vms/client/desktop/image_providers/camera_thumbnail_manager.h>

#include <core/resource_access/resource_access_filter.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/client/desktop/image_providers/layout_thumbnail_loader.h>

#include <nx/utils/log/log.h>

#include <ui/animation/variant_animator.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/resource_status_overlay_widget.h>
#include <ui/graphics/items/overlays/status_overlay_controller.h>
#include <ui/graphics/items/resource/videowall_screen_widget.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/processors/drag_processor.h>
#include <ui/processors/hover_processor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workaround/hidpi_workarounds.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/linear_combination.h>
#include <nx/client/core/utils/geometry.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using nx::vms::client::core::Geometry;

namespace {

/** Background color for overlay panels. */
const QColor overlayBackgroundColor = QColor(0, 0, 0, 96); // TODO: #Elric #customization
const QColor overlayTextColor = QColor(255, 255, 255, 160); // TODO: #Elric #customization

// We request this size for thumbnails.
static const QSize kPreviewSize(640, 480);
}

class QnVideowallItemWidgetHoverProgressAccessor: public AbstractAccessor
{
    virtual QVariant get(const QObject *object) const override
    {
        return static_cast<const QnVideowallItemWidget *>(object)->m_hoverProgress;
    }

    virtual void set(QObject *object, const QVariant &value) const override
    {
        QnVideowallItemWidget *widget = static_cast<QnVideowallItemWidget *>(object);
        if (qFuzzyEquals(widget->m_hoverProgress, value.toReal()))
            return;

        widget->m_hoverProgress = value.toReal();
        widget->update();
    }
};

QnVideowallItemWidget::QnVideowallItemWidget(
    const QnVideoWallResourcePtr &videowall,
    const QnUuid &itemUuid,
    QnVideowallScreenWidget *parent,
    QGraphicsWidget* parentWidget,
    Qt::WindowFlags windowFlags)
    :
    base_type(parentWidget, windowFlags),
    QnWorkbenchContextAware(parent),
    m_parentWidget(parentWidget),
    m_widget(parent),
    m_videowall(videowall),
    m_itemUuid(itemUuid),
    m_indices(QnVideoWallItemIndexList() << QnVideoWallItemIndex(m_videowall, m_itemUuid)),
    m_hoverProgress(0.0)
{
    setAcceptDrops(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setClickableButtons(Qt::LeftButton | Qt::RightButton);

    connect(m_videowall, &QnVideoWallResource::itemChanged, this,
        &QnVideowallItemWidget::at_videoWall_itemChanged);
    connect(this, &QnClickableWidget::doubleClicked, this,
        &QnVideowallItemWidget::at_doubleClicked);

    m_dragProcessor = new DragProcessor(this);
    m_dragProcessor->setHandler(this);

    m_frameColorAnimator = new VariantAnimator(this);
    m_frameColorAnimator->setTargetObject(this);
    m_frameColorAnimator->setAccessor(new QnVideowallItemWidgetHoverProgressAccessor());
    m_frameColorAnimator->setTimeLimit(200); // TODO: #GDM check value
    registerAnimation(m_frameColorAnimator);

    m_hoverProcessor = new HoverFocusProcessor(this);
    m_hoverProcessor->addTargetItem(this);
    m_hoverProcessor->setHoverEnterDelay(50);
    m_hoverProcessor->setHoverLeaveDelay(50);
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverEntered, this,
        [this]
        {
            updateHud(true);
        });
    connect(m_hoverProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            updateHud(true);
        });

    /* Status overlay. */
    const auto overlay = new QnStatusOverlayWidget(this);
    m_statusOverlayController = new QnStatusOverlayController(m_videowall, overlay, this);

    connect(m_statusOverlayController, &QnStatusOverlayController::statusOverlayChanged, this,
        [this, overlay, controller = m_statusOverlayController](bool animated)
        {
            const auto value = controller->statusOverlay() != Qn::EmptyOverlay;
            setOverlayWidgetVisible(overlay, value, animated);
        });

    addOverlayWidget(overlay, detail::OverlayParams(UserVisible, true));
    setOverlayWidgetVisible(overlay, false, false);

    initInfoOverlay();

    updateLayout();
    updateInfo();
}


QnVideowallItemWidget::~QnVideowallItemWidget()
{
}

void QnVideowallItemWidget::initInfoOverlay()
{
    /* Set up overlay widgets. */
    QFont font = this->font();
    font.setPixelSize(20);
    setFont(font);
    setPaletteColor(this, QPalette::WindowText, overlayTextColor);

    /* Header overlay. */
    m_headerLabel = new GraphicsLabel();
    m_headerLabel->setAcceptedMouseButtons(0);
    m_headerLabel->setPerformanceHint(GraphicsLabel::PixmapCaching);

    m_headerLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    m_headerLayout->setContentsMargins(10.0, 5.0, 5.0, 10.0);
    m_headerLayout->setSpacing(0.2);
    m_headerLayout->addItem(m_headerLabel);
//    m_headerLayout->addStretch(0x1000); /* Set large enough stretch for the buttons to be placed at the right end of the layout. */

    m_headerWidget = new GraphicsWidget();
    m_headerWidget->setLayout(m_headerLayout);
    m_headerWidget->setAcceptedMouseButtons(0);
    m_headerWidget->setAutoFillBackground(true);
    setPaletteColor(m_headerWidget, QPalette::Window, overlayBackgroundColor);

    QGraphicsLinearLayout *headerOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    headerOverlayLayout->setContentsMargins(10.0, 5.0, 5.0, 10.0);
    headerOverlayLayout->addItem(m_headerWidget);
    headerOverlayLayout->addStretch(0x1000);

    QGraphicsWidget* headerStretch = new QGraphicsWidget();
    headerStretch->setMinimumHeight(300);
    headerOverlayLayout->addItem(headerStretch);

    m_headerOverlayWidget = new QnViewportBoundWidget(m_parentWidget);
    m_headerOverlayWidget->setLayout(headerOverlayLayout);
    m_headerOverlayWidget->setAcceptedMouseButtons(0);
    addOverlayWidget(m_headerOverlayWidget, UserVisible);

    updateHud(false);
}

const QnResourceWidgetFrameColors &QnVideowallItemWidget::frameColors() const
{
    return m_frameColors;
}

void QnVideowallItemWidget::setFrameColors(const QnResourceWidgetFrameColors &frameColors)
{
    m_frameColors = frameColors;
    update();
}

void QnVideowallItemWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
    QWidget* /*widget*/)
{
    QRectF paintRect = option->rect;
    if (!paintRect.isValid())
        return;

    qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;

    paintRect.adjust(offset * 2, offset * 2, -offset * 2, -offset * 2);
    painter->fillRect(paintRect, palette().color(QPalette::Window));

    m_statusOverlayController->setStatusOverlay(m_resourceStatus, true);

    if (m_layout && !m_layoutThumbnail.isNull())
    {
        QSizeF paintSize = m_layoutThumbnail.size() / m_layoutThumbnail.devicePixelRatio();
        // Fitting thumbnail exactly to widget's rect.
        paintSize = Geometry::bounded(paintSize, paintRect.size(), Qt::KeepAspectRatio);
        paintSize = Geometry::expanded(paintSize, paintRect.size(), Qt::KeepAspectRatio);

        QRect dstRect = QStyle::alignedRect(layoutDirection(), Qt::AlignCenter,
            paintSize.toSize(), paintRect.toRect());

        painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        painter->drawPixmap(dstRect, m_layoutThumbnail);
    }

    paintFrame(painter, paintRect);
}

void QnVideowallItemWidget::paintFrame(QPainter *painter, const QRectF &paintRect)
{
    if (!paintRect.isValid())
        return;

    qreal offset = qMin(paintRect.width(), paintRect.height()) * 0.02;
    qreal m_frameOpacity = 1.0;
    qreal m_frameWidth = offset;

    QColor color = linearCombine(m_hoverProgress, m_frameColors.selected, 1 - m_hoverProgress, m_frameColors.normal);

    qreal w = paintRect.width();
    qreal h = paintRect.height();
    qreal fw = m_frameWidth;
    qreal x = paintRect.x();
    qreal y = paintRect.y();

    QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * m_frameOpacity);

    /* Antialiasing is here for a reason. Without it border looks crappy. */
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    painter->fillRect(QRectF(x - fw, y - fw, w + fw * 2, fw), color);
    painter->fillRect(QRectF(x - fw, y + h, w + fw * 2, fw), color);
    painter->fillRect(QRectF(x - fw, y, fw, h), color);
    painter->fillRect(QRectF(x + w, y, fw, h), color);
}

void QnVideowallItemWidget::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    m_mimeData.reset(new MimeData{event->mimeData(), resourcePool()});
    if (!isDragValid())
        return;

    event->acceptProposedAction();
}

void QnVideowallItemWidget::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (!isDragValid())
        return;

    m_hoverProcessor->forceHoverEnter();
    event->acceptProposedAction();
}

void QnVideowallItemWidget::dragLeaveEvent(QGraphicsSceneDragDropEvent* /*event*/)
{
    m_mimeData.reset();
    m_hoverProcessor->forceHoverLeave();
}

void QnVideowallItemWidget::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    action::Parameters parameters;

    const auto videoWallItems = resourcePool()->getVideoWallItemsByUuid(m_mimeData->entities());
    if (!videoWallItems.isEmpty())
    {
        parameters = videoWallItems;
    }
    else
    {
        parameters = m_mimeData->resources();
        resourcePool()->addNewResources(m_mimeData->resources());
    }

    parameters.setArgument(Qn::VideoWallItemGuidRole, m_itemUuid);
    parameters.setArgument(Qn::KeyboardModifiersRole, event->modifiers());

    menu()->trigger(action::DropOnVideoWallItemAction, parameters);

    m_mimeData.reset();
    event->acceptProposedAction();
}

void QnVideowallItemWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mousePressEvent(event);
    if (event->button() != Qt::LeftButton)
        return;

    m_dragProcessor->mousePressEvent(this, event);
}


void QnVideowallItemWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mouseMoveEvent(event);
    m_dragProcessor->mouseMoveEvent(this, event);
}

void QnVideowallItemWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton)
        return;

    m_dragProcessor->mouseReleaseEvent(this, event);
}

void QnVideowallItemWidget::startDrag(DragInfo *info)
{
    MimeData data;
    data.setEntities({m_itemUuid});
    data.setData(Qn::NoSceneDrop, QByteArray());

    auto drag = new QDrag(this);
    drag->setMimeData(data.createMimeData());

    Qt::DropAction dropAction = Qt::MoveAction;
    if (info && info->modifiers() & Qt::ControlModifier)
        dropAction = Qt::CopyAction;

    drag->exec(dropAction, dropAction);

    m_dragProcessor->reset();
}

void QnVideowallItemWidget::clickedNotify(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::RightButton)
        return;

    QScopedPointer<QMenu> popupMenu(menu()->newMenu(action::SceneScope, nullptr, m_indices));

    if (popupMenu->isEmpty())
        return;

    QnHiDpiWorkarounds::showMenu(popupMenu.data(), QCursor::pos());
}

void QnVideowallItemWidget::at_videoWall_itemChanged(const QnVideoWallResourcePtr& videoWall,
    const QnVideoWallItem& item)
{
    Q_UNUSED(videoWall)
    if (item.uuid != m_itemUuid)
        return;
    updateLayout();
    updateInfo();
}

void QnVideowallItemWidget::at_doubleClicked(Qt::MouseButton button)
{
    if (button != Qt::LeftButton)
        return;

    menu()->triggerIfPossible(action::StartVideoWallControlAction, m_indices);
}

void QnVideowallItemWidget::updateLayout()
{
    QnVideoWallItem item = m_videowall->items()->getItem(m_itemUuid);
    auto layout = resourcePool()->getResourceById<QnLayoutResource>(item.layout);
    if (m_layout == layout)
        return;

    if (m_layout)
    {
        // Disconnect from previous layout
        m_layout->disconnect(this);
        m_layoutThumbnailProvider.reset();
        m_resourceStatus = Qn::NoDataOverlay;
    }

    m_layout = layout;

    if (m_layout)
    {
        // Right now this widget does not have any sort of proper size to calculate aspect ratio
        const QSize previewSize = kPreviewSize;
        m_layoutThumbnailProvider.reset(
            new LayoutThumbnailLoader(m_layout, previewSize, nx::api::ImageRequest::kLatestThumbnail));

        connect(m_layoutThumbnailProvider.get(), &ImageProvider::statusChanged,
            this, &QnVideowallItemWidget::at_updateThumbnailStatus);

        m_layoutThumbnailProvider->setResourcePool(resourcePool());
        m_layoutThumbnailProvider->loadAsync();

        connect(m_layout, &QnLayoutResource::itemAdded, this,
            &QnVideowallItemWidget::updateView);
        connect(m_layout, &QnLayoutResource::itemChanged, this,
            &QnVideowallItemWidget::updateView);
        connect(m_layout, &QnLayoutResource::itemRemoved, this,
            &QnVideowallItemWidget::updateView);
        connect(m_layout, &QnLayoutResource::cellAspectRatioChanged, this,
            &QnVideowallItemWidget::updateView);
        connect(m_layout, &QnLayoutResource::cellSpacingChanged, this,
            &QnVideowallItemWidget::updateView);
        connect(m_layout, &QnLayoutResource::backgroundImageChanged, this,
            &QnVideowallItemWidget::updateView);
        connect(m_layout, &QnLayoutResource::backgroundSizeChanged, this,
            &QnVideowallItemWidget::updateView);
        connect(m_layout, &QnLayoutResource::backgroundOpacityChanged, this,
            &QnVideowallItemWidget::updateView);
    }
}

void QnVideowallItemWidget::updateView()
{
    update(); //direct connect is not so convenient because of overloaded function
}

void QnVideowallItemWidget::updateInfo()
{
    m_headerLabel->setText(m_videowall->items()->getItem(m_itemUuid).name);
    updateHud(false);
    // TODO: #GDM #VW update layout in case of transition "long name -> short name"
}

void QnVideowallItemWidget::updateHud(bool animate)
{
    bool headerVisible = m_hoverProcessor->isHovered();
    setOverlayWidgetVisible(m_headerOverlayWidget, headerVisible, animate);

    qreal frameColorValue = headerVisible ? 1.0 : 0.0;
    if (animate)
        m_frameColorAnimator->animateTo(frameColorValue);
    else
        m_hoverProgress = frameColorValue;

    update();
}

bool QnVideowallItemWidget::isDragValid() const
{
    return !m_mimeData->isEmpty();
}

void QnVideowallItemWidget::at_updateThumbnailStatus(Qn::ThumbnailStatus status)
{
    switch (status)
    {
        case Qn::ThumbnailStatus::Loaded:
            m_resourceStatus = Qn::EmptyOverlay;
            m_layoutThumbnail = QPixmap::fromImage(m_layoutThumbnailProvider->image());
            NX_VERBOSE(this) << "QnVideowallItemWidget got thumbs of size " << m_layoutThumbnail.size();
            update();
            break;

        case Qn::ThumbnailStatus::Loading:
            m_resourceStatus = Qn::LoadingOverlay;
            break;

        default:
            m_resourceStatus = Qn::NoDataOverlay;
            break;
    }
}
