#pragma once

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
class QnStatusOverlayController;
class QnImageButtonWidget;
class QnViewportBoundWidget;

class GraphicsLabel;
class GraphicsWidget;
class QGraphicsLinearLayout;

namespace nx {
namespace client {
namespace desktop {

class MimeData;

} // namespace desktop
} // namespace client
} // namespace nx

class QnVideowallItemWidget:
    public Overlayed<Animated<Connective<QnClickableWidget>>>,
    protected DragProcessHandler,
    public QnWorkbenchContextAware
{
    typedef Overlayed<Animated<Connective<QnClickableWidget>>> base_type;
    Q_OBJECT

    Q_PROPERTY(QnResourceWidgetFrameColors frameColors READ frameColors WRITE setFrameColors)
public:
    explicit QnVideowallItemWidget(const QnVideoWallResourcePtr &videowall, const QnUuid &itemUuid, QnVideowallScreenWidget *parent, QGraphicsWidget* parentWidget, Qt::WindowFlags windowFlags = 0);
    virtual ~QnVideowallItemWidget() override;

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
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item);
    void at_doubleClicked(Qt::MouseButton button);

    void initInfoOverlay();

    void updateLayout();
    void updateView();
    void updateInfo();
    void updateHud(bool animate);

    /** \returns false if item image is still loading */
    bool paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data);

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
    const QnUuid m_itemUuid;

    /** Cached field to be used as action parameters source. */
    const QnVideoWallItemIndexList m_indices;

    /** Attached layout resource (if any). */
    QnLayoutResourcePtr m_layout;

    /** Drag'n'drop support structure. */
    std::unique_ptr<nx::client::desktop::MimeData> m_mimeData;

    QnResourceWidgetFrameColors m_frameColors;

    VariantAnimator* m_frameColorAnimator;
    qreal m_hoverProgress;

    /** Status overlay. */
    QnStatusOverlayController* m_statusOverlayController;

    /* Widgets for overlaid stuff. */
    QnViewportBoundWidget *m_headerOverlayWidget;
    QGraphicsLinearLayout *m_headerLayout;
    GraphicsWidget *m_headerWidget;
    GraphicsLabel *m_headerLabel;
};
