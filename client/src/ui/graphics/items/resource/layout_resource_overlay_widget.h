#ifndef LAYOUT_RESOURCE_OVERLAY_WIDGET_H
#define LAYOUT_RESOURCE_OVERLAY_WIDGET_H

#include <client/client_color_types.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/layout_item_data.h>
#include <core/resource/videowall_item_index.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/processors/drag_process_handler.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class DragProcessor;
class HoverFocusProcessor;
class QnVideowallResourceScreenWidget;
class VariantAnimator;

class QnLayoutResourceOverlayWidget : public Connective<QnClickableWidget>, protected DragProcessHandler, public QnWorkbenchContextAware {
    typedef Connective<QnClickableWidget> base_type;
    Q_OBJECT

    Q_PROPERTY(QnResourceWidgetFrameColors frameColors READ frameColors WRITE setFrameColors)

public:
    explicit QnLayoutResourceOverlayWidget(const QnVideoWallResourcePtr &videowall, const QUuid &itemUuid, QnVideowallResourceScreenWidget *parent, Qt::WindowFlags windowFlags = 0);

    const QnResourceWidgetFrameColors &frameColors() const;
    void setFrameColors(const QnResourceWidgetFrameColors &frameColors);

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

    void updateLayout();
    void updateView();
    void updateInfo();

    void paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data);
private:
    friend class QnVideowallResourceScreenWidget;
    friend class QnLayoutResourceOverlayWidgetHoverProgressAccessor;

    /** Parent widget */
    QnVideowallResourceScreenWidget* m_widget;

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
};

#endif // LAYOUT_RESOURCE_OVERLAY_WIDGET_H
