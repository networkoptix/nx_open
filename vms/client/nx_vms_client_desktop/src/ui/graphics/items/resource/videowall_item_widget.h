// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_widget.h>

#include <client/client_globals.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <ui/animation/animated.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/overlays/overlayed.h>
#include <ui/processors/drag_process_handler.h>

class DragProcessor;
class VariantAnimator;
class HoverFocusProcessor;

class QnVideowallScreenWidget;
class QnStatusOverlayController;
class QnImageButtonWidget;
class QnViewportBoundWidget;

class GraphicsLabel;
class GraphicsWidget;
class QGraphicsLinearLayout;

namespace nx::vms::client::desktop {

class MimeData;
class LayoutThumbnailLoader;

} // namespace nx::vms::client::desktop

class QnVideowallItemWidget:
    public Overlayed<Animated<QnClickableWidget>>,
    protected DragProcessHandler
{
    typedef Overlayed<Animated<QnClickableWidget>> base_type;
    Q_OBJECT

public:
    explicit QnVideowallItemWidget(
        const QnVideoWallResourcePtr& videowall,
        const nx::Uuid& itemUuid,
        QnVideowallScreenWidget* parent,
        QGraphicsWidget* parentWidget,
        Qt::WindowFlags windowFlags = {});
    virtual ~QnVideowallItemWidget() override;

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paintFrame(QPainter *painter, const QRectF &paintRect);

    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override;
    virtual void dropEvent(QGraphicsSceneDragDropEvent *event) override;

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    virtual void startDrag(DragInfo *info) override;

    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) override;

private:
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);

    void at_updateThumbnailStatus(Qn::ThumbnailStatus status);

    void initInfoOverlay();

    void updateLayout();
    void updateView();
    void updateInfo();
    void updateHud(bool animate);

    bool isDragValid() const;

private:
    friend class QnVideowallScreenWidget;
    friend class QnVideowallItemWidgetHoverProgressAccessor;

    /** Parent widget */
    QGraphicsWidget* m_parentWidget;

    QnVideowallScreenWidget* m_widget;

    DragProcessor *m_dragProcessor;
    HoverFocusProcessor* m_hoverProcessor;

    /** Parent videowall resource. */
    const QnVideoWallResourcePtr m_videowall;

    /** Uuid of the current screen instance. */
    const nx::Uuid m_itemUuid;

    /** Cached field to be used as action parameters source. */
    const QnVideoWallItemIndexList m_indices;

    /** Attached layout resource (if any). */
    QnLayoutResourcePtr m_layout;

    /** Drag'n'drop support structure. */
    std::unique_ptr<nx::vms::client::desktop::MimeData> m_mimeData;

    VariantAnimator* m_frameColorAnimator;
    qreal m_hoverProgress;

    /** Status overlay. */
    QnStatusOverlayController* m_statusOverlayController;

    /* Widgets for overlaid stuff. */
    QnViewportBoundWidget *m_headerOverlayWidget;
    QGraphicsLinearLayout *m_headerLayout;
    GraphicsWidget *m_headerWidget;
    GraphicsLabel *m_headerLabel;

    QPixmap m_layoutThumbnail;

    // Status of resource loading.
    Qn::ResourceStatusOverlay m_resourceStatus = Qn::NoDataOverlay;

    std::unique_ptr<nx::vms::client::desktop::LayoutThumbnailLoader> m_layoutThumbnailProvider;
};
