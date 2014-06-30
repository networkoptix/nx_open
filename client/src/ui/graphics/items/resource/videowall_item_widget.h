#ifndef LAYOUT_RESOURCE_OVERLAY_WIDGET_H
#define LAYOUT_RESOURCE_OVERLAY_WIDGET_H

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/videowall_item_index.h>

#include <ui/animation/animated.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/overlays/overlayed.h>
#include <ui/processors/drag_process_handler.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class DragProcessor;
class VariantAnimator;
class HoverFocusProcessor;

class QnVideowallScreenWidget;
class QnStatusOverlayWidget;
class QnImageButtonWidget;
class QnViewportBoundWidget;

class GraphicsLabel;
class GraphicsWidget;

class QnVideowallItemWidget : public Overlayed<Animated<Connective<QnClickableWidget>>>, protected DragProcessHandler, public QnWorkbenchContextAware {
    typedef Overlayed<Animated<Connective<QnClickableWidget>>> base_type;
    Q_OBJECT

    Q_PROPERTY(QnResourceWidgetFrameColors frameColors READ frameColors WRITE setFrameColors)
public:
    explicit QnVideowallItemWidget(const QnVideoWallResourcePtr &videowall, const QUuid &itemUuid, QnVideowallScreenWidget *parent, QGraphicsWidget* parentWidget, Qt::WindowFlags windowFlags = 0);

    const QnResourceWidgetFrameColors &frameColors() const;
    void setFrameColors(const QnResourceWidgetFrameColors &frameColors);

    bool isInfoVisible() const;
    void setInfoVisible(bool visible, bool animate = true);

signals:
    void infoVisibleChanged(bool visible);

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
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void at_doubleClicked(Qt::MouseButton button);
    void at_infoButton_toggled(bool toggled);

    void initInfoOverlay();

    void updateLayout();
    void updateView();
    void updateInfo();
    void updateStatusOverlay(Qn::ResourceStatusOverlay overlay);

    /** \returns false if item image is still loading */
    bool paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data);
    
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
    const QUuid m_itemUuid;

    /** Cached field to be used as action parameters source. */
    const QnVideoWallItemIndexList m_indices;

    /** Attached layout resource (if any). */
    QnLayoutResourcePtr m_layout;

    /** Drag'n'drop support structure. */
    struct {
        QnResourceList resources;
        QnVideoWallItemIndexList videoWallItems;
    } m_dragged;

    QnResourceWidgetFrameColors m_frameColors;

    VariantAnimator* m_frameColorAnimator;
    qreal m_hoverProgress;

    /** Status overlay. */
    QnStatusOverlayWidget* m_statusOverlayWidget;

    /** Info overlay */
    bool m_infoVisible;

    /* Widgets for overlaid stuff. */
    QnViewportBoundWidget *m_headerOverlayWidget;
    QGraphicsLinearLayout *m_headerLayout;
    GraphicsWidget *m_headerWidget;
    GraphicsLabel *m_headerLabel;
    QnImageButtonWidget* m_infoButton;

    QnViewportBoundWidget *m_footerOverlayWidget;
    GraphicsWidget *m_footerWidget;
    GraphicsLabel *m_footerLabel;
};

#endif // LAYOUT_RESOURCE_OVERLAY_WIDGET_H
