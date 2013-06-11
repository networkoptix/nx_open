#include "zoom_window_instrument.h"

#include <cassert>

#include <utils/math/color_transformations.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/hash.h>
#include <utils/common/util.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/constrained_geometrically.h>
#include <ui/common/constrained_resizable.h>
#include <ui/style/globals.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench.h>

#include "instrument_manager.h"
#include "resizing_instrument.h"
#include "selection_item.h"

namespace {
    const qreal zoomFrameWidth = qnGlobals->workbenchUnitSize() * 0.005; // TODO: #Elric move to settings;

    const qreal zoomWindowMinSize = 0.1;
    const qreal zoomWindowMaxSize = 0.9;

} // anonymous namespace

class ZoomOverlayWidget;


// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
class ZoomWindowWidget: public Instrumented<QnClickableWidget>, public ConstrainedGeometrically {
    typedef Instrumented<QnClickableWidget> base_type;
public:
    ZoomWindowWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setWindowFrameMargins(zoomFrameWidth, zoomFrameWidth, zoomFrameWidth, zoomFrameWidth);

        setAcceptedMouseButtons(Qt::LeftButton);
        setClickableButtons(Qt::LeftButton);

        setWindowFlags(this->windowFlags() | Qt::Window);
        setFlag(ItemIsPanel, false); /* See comment in workbench_display.cpp. */
    }

    virtual ~ZoomWindowWidget();

    ZoomOverlayWidget *overlay() const {
        return m_overlay.data();
    }

    void setOverlay(ZoomOverlayWidget *overlay) {
        m_overlay = overlay;
    }

    QnMediaResourceWidget *zoomWidget() const {
        return m_zoomWidget.data();
    }

    void setZoomWidget(QnMediaResourceWidget *zoomWidget) {
        m_zoomWidget = zoomWidget;
    }

    QColor frameColor() const {
        return m_frameColor;
    }

    void setFrameColor(const QColor &frameColor) {
        m_frameColor = frameColor;
    }

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override {
        qreal scale = std::pow(2.0, (-event->delta() / 8.0) / 180.0);

        QRectF geometry = this->geometry();
        QPointF fixedPoint = mapToParent(event->pos());
        geometry = constrainedGeometry(
            QnGeometry::scaled(
                geometry,
                geometry.size() * scale,
                fixedPoint,
                Qt::IgnoreAspectRatio
            ),
            Qn::NoCorner,
            fixedPoint
        );

        setGeometry(geometry);
        
        event->accept();
    }

    virtual QRectF constrainedGeometry(const QRectF &geometry, Qn::Corner pinCorner, const QPointF &pinPoint = QPointF()) const override;

    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        qreal l, t, r, b;
        getWindowFrameMargins(&l, &t, &r, &b);

        QSizeF size = this->size();
        qreal w = size.width();
        qreal h = size.height();
        QColor color = toTransparent(m_frameColor, 0.75);

        painter->fillRect(QRectF(-l,      -t,      w + l + r,   t), color);
        painter->fillRect(QRectF(-l,      h,       w + l + r,   b), color);
        painter->fillRect(QRectF(-l,      0,       l,           h), color);
        painter->fillRect(QRectF(w,       0,       r,           h), color);
    }

    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override {
        Qn::WindowFrameSections result = base_type::windowFrameSectionsAt(region);

        if(result & Qn::SideSections)
            result &= ~Qn::SideSections;
        if(result == Qn::NoSection)
            result = Qn::TitleBarArea;

        return result;
    }

private:
    QColor m_frameColor;
    QWeakPointer<ZoomOverlayWidget> m_overlay;
    QWeakPointer<QnMediaResourceWidget> m_zoomWidget;
};


// -------------------------------------------------------------------------- //
// ZoomOverlayWidget
// -------------------------------------------------------------------------- //
class ZoomOverlayWidget: public GraphicsWidget {
    typedef GraphicsWidget base_type;
public:
    ZoomOverlayWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags)
    {
        setAcceptedMouseButtons(0);
    }

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target) {
        m_target = target;
    }

    void addWidget(ZoomWindowWidget *widget) {
        widget->setOverlay(this);
        widget->setParentItem(this);
        m_rectByWidget.insert(widget, QRectF(0.0, 0.0, 1.0, 1.0));
        
        updateLayout(widget);
    }

    void removeWidget(ZoomWindowWidget *widget) {
        if(widget->parentItem() == this) {
            widget->setParentItem(NULL);
            widget->setOverlay(NULL);
        }
        m_rectByWidget.remove(widget);
    }
    
    void setWidgetRect(ZoomWindowWidget *widget, const QRectF &rect) {
        if(!m_rectByWidget.contains(widget))
            return;

        m_rectByWidget[widget] = rect;

        updateLayout(widget);
    }

    QRectF widgetRect(ZoomWindowWidget *widget) const {
        return m_rectByWidget.value(widget, QRectF());
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyCompare(oldSize, size()))
            updateLayout();
    }

private:
    void updateLayout() {
        foreach(ZoomWindowWidget *widget, m_rectByWidget.keys())
            updateLayout(widget);
    }

    void updateLayout(ZoomWindowWidget *widget) {
        widget->setGeometry(QnGeometry::subRect(rect(), m_rectByWidget.value(widget)));
    }

private:
    QHash<ZoomWindowWidget *, QRectF> m_rectByWidget;
    QWeakPointer<QnMediaResourceWidget> m_target;
};


// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
ZoomWindowWidget::~ZoomWindowWidget() {
    if(overlay())
        overlay()->removeWidget(this);
}

QRectF ZoomWindowWidget::constrainedGeometry(const QRectF &geometry, Qn::Corner pinCorner, const QPointF &pinPoint) const {
    ZoomOverlayWidget *overlayWidget = this->overlay();
    QRectF result = geometry;

    /* Size constraints go first. */
    QSizeF maxSize = geometry.size();
    if(overlayWidget)
        maxSize = QnGeometry::cwiseMax(QnGeometry::cwiseMin(maxSize, overlayWidget->size() * zoomWindowMaxSize), overlayWidget->size() * zoomWindowMinSize);
    result = ConstrainedResizable::constrainedGeometry(geometry, pinCorner, pinPoint, QnGeometry::expanded(QnGeometry::aspectRatio(size()), maxSize, Qt::KeepAspectRatio));

    /* Position constraints go next. */
    if(overlayWidget) {
        if(pinCorner != Qn::NoCorner) {
            QRectF constraint = overlayWidget->rect();
            QPointF pinPoint = QnGeometry::corner(geometry, pinCorner);

            qreal xScaleFactor = 1.0;
            if(result.left() < constraint.left() && !qFuzzyCompare(result.left(), pinPoint.x())) {
                xScaleFactor = (constraint.left() - pinPoint.x()) / (result.left() - pinPoint.x());
            } else if(result.right() > constraint.right() && !qFuzzyCompare(result.right(), pinPoint.x())) {
                xScaleFactor = (constraint.right() - pinPoint.x()) / (result.right() - pinPoint.x());
            }

            qreal yScaleFactor = 1.0;
            if(result.top() < constraint.top() && !qFuzzyCompare(result.top(), pinPoint.y())) {
                yScaleFactor = (constraint.top() - pinPoint.y()) / (result.top() - pinPoint.y());
            } else if(result.bottom() > constraint.bottom() && !qFuzzyCompare(result.bottom(), pinPoint.y())) {
                yScaleFactor = (constraint.bottom() - pinPoint.y()) / (result.bottom() - pinPoint.y());
            }

            qreal scaleFactor = qMin(xScaleFactor, yScaleFactor);
            result = ConstrainedResizable::constrainedGeometry(result, pinCorner, pinPoint, result.size() * scaleFactor);
        } else {
            result = QnGeometry::movedInto(result, overlayWidget->rect());
        }
    }

    return result;
}



// -------------------------------------------------------------------------- //
// ZoomWindowInstrument
// -------------------------------------------------------------------------- //
ZoomWindowInstrument::ZoomWindowInstrument(QObject *parent):
    base_type(
        makeSet(QEvent::MouseButtonPress),
        makeSet(),
        makeSet(),
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), 
        parent
    ),
    QnWorkbenchContextAware(parent)
{
    m_zoomWindowColors = qnGlobals->zoomWindowColors();

    connect(display(), SIGNAL(zoomLinkAdded(QnResourceWidget *, QnResourceWidget *)), this, SLOT(at_display_zoomLinkAdded(QnResourceWidget *, QnResourceWidget *)));
    connect(display(), SIGNAL(zoomLinkAboutToBeRemoved(QnResourceWidget *, QnResourceWidget *)), this, SLOT(at_display_zoomLinkAboutToBeRemoved(QnResourceWidget *, QnResourceWidget *)));
    connect(display(), SIGNAL(widgetChanged(Qn::ItemRole)), this, SLOT(at_display_widgetChanged(Qn::ItemRole)));
}

ZoomWindowInstrument::~ZoomWindowInstrument() {
    ensureUninstalled();
}

QColor ZoomWindowInstrument::nextZoomWindowColor() const {
    QSet<QColor> colors;
    foreach(QnResourceWidget *widget, display()->widgets())
        colors.insert(widget->frameColor());

    foreach(const QColor &color, m_zoomWindowColors)
        if(!colors.contains(color))
            return color;

    return m_zoomWindowColors[random(0, m_zoomWindowColors.size())];
}

ZoomOverlayWidget *ZoomWindowInstrument::overlayWidget(QnMediaResourceWidget *widget) const {
    return m_dataByWidget[widget].overlayWidget;
}

ZoomOverlayWidget *ZoomWindowInstrument::ensureOverlayWidget(QnMediaResourceWidget *widget) {
    ZoomData &data = m_dataByWidget[widget];

    ZoomOverlayWidget *overlayWidget = data.overlayWidget;
    if(overlayWidget)
        return overlayWidget;

    overlayWidget = new ZoomOverlayWidget();
    overlayWidget->setOpacity(1.0);
    overlayWidget->setTarget(widget);
    data.overlayWidget = overlayWidget;

    widget->addOverlayWidget(overlayWidget, QnResourceWidget::AutoVisible, false, false, false);
    updateOverlayVisibility(widget);

    return overlayWidget;
}

ZoomWindowWidget *ZoomWindowInstrument::windowWidget(QnMediaResourceWidget *widget) const {
    return m_dataByWidget[widget].windowWidget;
}

void ZoomWindowInstrument::ensureSelectionItem() {
    if(selectionItem())
        return;

    m_selectionItem = new FixedArSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setPen(QPen(Qt::white, zoomFrameWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    selectionItem()->setBrush(Qt::NoBrush);
    selectionItem()->setElementSize(qnGlobals->workbenchUnitSize() / 64.0);
    selectionItem()->setOptions(FixedArSelectionItem::DrawSideElements);

    if(scene())
        scene()->addItem(selectionItem());
}

void ZoomWindowInstrument::registerWidget(QnMediaResourceWidget *widget) {
    connect(widget, SIGNAL(zoomRectChanged()), this, SLOT(at_widget_zoomRectChanged()));
    connect(widget, SIGNAL(frameColorChanged()), this, SLOT(at_widget_frameColorChanged()));
    connect(widget, SIGNAL(aboutToBeDestroyed()), this, SLOT(at_widget_aboutToBeDestroyed()));
    connect(widget, SIGNAL(optionsChanged()), this, SLOT(at_widget_optionsChanged()));

    // TODO: #Elric hack =(
    if(!widget->zoomRect().isNull() && widget->frameColor() == qnGlobals->frameColor())
        widget->setFrameColor(nextZoomWindowColor());
}

void ZoomWindowInstrument::unregisterWidget(QnMediaResourceWidget *widget) {
    if(!m_dataByWidget.contains(widget))
        return; /* Double unregistration is possible and is OK. */

    disconnect(widget, NULL, this, NULL);

    m_dataByWidget.remove(widget);
}

void ZoomWindowInstrument::registerLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget) {
    ZoomData &data = m_dataByWidget[widget];

    ZoomOverlayWidget *overlayWidget = ensureOverlayWidget(zoomTargetWidget);

    ZoomWindowWidget *windowWidget;
    if(m_storedWindowWidget) {
        windowWidget = m_storedWindowWidget.data();
        m_storedWindowWidget.clear();
        windowWidget->show();
    } else {
        windowWidget = new ZoomWindowWidget();
    }
    windowWidget->setZoomWidget(widget);
    overlayWidget->addWidget(windowWidget);
    data.windowWidget = windowWidget;
    connect(windowWidget, SIGNAL(geometryChanged()), this, SLOT(at_windowWidget_geometryChanged()));
    connect(windowWidget, SIGNAL(doubleClicked()), this, SLOT(at_windowWidget_doubleClicked()));

    updateWindowFromWidget(widget);
}

void ZoomWindowInstrument::unregisterLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget, bool deleteWindowWidget) {
    Q_UNUSED(zoomTargetWidget)
    ZoomData &data = m_dataByWidget[widget];

    if(deleteWindowWidget) {
        delete data.windowWidget;
    } else {
        data.windowWidget->overlay()->removeWidget(data.windowWidget);
        data.windowWidget->setZoomWidget(NULL);
        disconnect(data.windowWidget, NULL, this, NULL);
    }
    data.windowWidget = NULL;
}

void ZoomWindowInstrument::updateOverlayVisibility(QnMediaResourceWidget *widget) {
    ZoomOverlayWidget *overlayWidget = this->overlayWidget(widget);
    if(!overlayWidget)
        return;

    QnResourceWidget::OverlayVisibility visibility;
    if(widget == display()->widget(Qn::ZoomedRole)) {
        visibility = QnResourceWidget::Invisible;
    } else if(widget->options() & (QnResourceWidget::DisplayMotion | QnResourceWidget::DisplayMotionSensitivity | QnResourceWidget::DisplayCrosshair)) {
        visibility = QnResourceWidget::Invisible;
    } else {
        visibility = QnResourceWidget::Visible;
    }
    widget->setOverlayWidgetVisibility(overlayWidget, visibility);
}

void ZoomWindowInstrument::updateWindowFromWidget(QnMediaResourceWidget *widget) {
    if(m_processingWidgets.contains(widget))
        return;
    m_processingWidgets.insert(widget);

    ZoomWindowWidget *windowWidget = this->windowWidget(widget);
    ZoomOverlayWidget *overlayWidget = windowWidget ? windowWidget->overlay() : NULL;

    if(windowWidget && overlayWidget) {
        overlayWidget->setWidgetRect(windowWidget, widget->zoomRect());
        windowWidget->setFrameColor(widget->frameColor().lighter(110));
    }

    m_processingWidgets.remove(widget);
}

void ZoomWindowInstrument::updateWidgetFromWindow(ZoomWindowWidget *windowWidget) {
    ZoomOverlayWidget *overlayWidget = windowWidget->overlay();
    QnMediaResourceWidget *zoomWidget = windowWidget->zoomWidget();

    if(m_processingWidgets.contains(zoomWidget))
        return;
    m_processingWidgets.insert(zoomWidget);

    if(overlayWidget && zoomWidget && !qFuzzyIsNull(overlayWidget->size())) {
        QRectF zoomRect = cwiseDiv(windowWidget->geometry(), overlayWidget->size());
        overlayWidget->setWidgetRect(windowWidget, zoomRect);
        emit zoomRectChanged(zoomWidget, zoomRect);
    }

    m_processingWidgets.remove(zoomWidget);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void ZoomWindowInstrument::installedNotify() {
    assert(selectionItem() == NULL);

    if(ResizingInstrument *resizingInstrument = manager()->instrument<ResizingInstrument>()) {
        connect(resizingInstrument, SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)), this, SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
        connect(resizingInstrument, SIGNAL(resizing(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)), this, SLOT(at_resizing(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
        connect(resizingInstrument, SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)), this, SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
    }

    base_type::installedNotify();
}

void ZoomWindowInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(ResizingInstrument *resizingInstrument = manager()->instrument<ResizingInstrument>())
        disconnect(resizingInstrument, NULL, this, NULL);

    if(selectionItem())
        delete selectionItem();
}

bool ZoomWindowInstrument::registeredNotify(QGraphicsItem *item) {
    if(!base_type::registeredNotify(item))
        return false;

    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item)) {
        registerWidget(widget);
        return true;
    }

    return false;
}

void ZoomWindowInstrument::unregisteredNotify(QGraphicsItem *item) {
    base_type::unregisteredNotify(item);

    /* Note that this dynamic_cast may fail because media resource widget is
     * already being destroyed. We don't care as in this case it will already
     * be unregistered in aboutToBeDestroyed handler. */
    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item))
        unregisterWidget(widget);
}

bool ZoomWindowInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *) {
    m_viewport = viewport;

    return false;
}

bool ZoomWindowInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    if(!(target->options() & QnResourceWidget::ControlZoomWindow))
        return false;

    if(!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    m_target = target;

    dragProcessor()->mousePressEvent(target, event);

    event->accept();
    return false;
}

void ZoomWindowInstrument::startDragProcess(DragInfo *) {
    emit zoomWindowProcessStarted(target());
}

void ZoomWindowInstrument::startDrag(DragInfo *) {
    m_zoomWindowStartedEmitted = false;

    if(!target()) {
        /* Whoops, already destroyed. */
        reset();
        return;
    }

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    selectionItem()->setViewport(m_viewport.data());
    opacityAnimator(selectionItem())->stop();
    selectionItem()->setOpacity(1.0);
    
    m_zoomWindowColor = nextZoomWindowColor();
    selectionItem()->setColor(toTransparent(m_zoomWindowColor, 0.75));

    /* Everything else will be initialized in the first call to drag(). */

    emit zoomWindowStarted(target());
    m_zoomWindowStartedEmitted = true;
}

void ZoomWindowInstrument::dragMove(DragInfo *info) {
    if(!target()) {
        reset();
        return;
    }

    ensureSelectionItem();
    selectionItem()->setGeometry(info->mousePressItemPos(), info->mouseItemPos(), aspectRatio(target()->size()) / aspectRatio(target()->channelLayout()->size()), target()->rect());
}

void ZoomWindowInstrument::finishDrag(DragInfo *) {
    if(target()) {
        ensureSelectionItem();
        opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

        QRectF zoomRect = cwiseDiv(selectionItem()->rect(), target()->size());
        if(zoomRect.width() <= zoomWindowMinSize || zoomRect.height() <= zoomWindowMinSize) {
            zoomRect = movedInto(
                expanded(aspectRatio(zoomRect), QSizeF(zoomWindowMinSize, zoomWindowMinSize), zoomRect.center(), Qt::KeepAspectRatioByExpanding),
                QRectF(0.0, 0.0, 1.0, 1.0)
            );
        }
        if(zoomRect.width() >= zoomWindowMaxSize || zoomRect.height() >= zoomWindowMaxSize)
            zoomRect = expanded(aspectRatio(zoomRect), QSizeF(zoomWindowMaxSize, zoomWindowMaxSize), zoomRect.center(), Qt::KeepAspectRatio);
        
        emit zoomRectCreated(target(), m_zoomWindowColor, zoomRect);
    }

    if(m_zoomWindowStartedEmitted)
        emit zoomWindowFinished(target());
}

void ZoomWindowInstrument::finishDragProcess(DragInfo *) {
    emit zoomWindowProcessFinished(target());
}

void ZoomWindowInstrument::at_widget_aboutToBeDestroyed() {
    unregisterWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_zoomRectChanged() {
    updateWindowFromWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_frameColorChanged() {
    updateWindowFromWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_optionsChanged() {
    updateOverlayVisibility(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_windowWidget_geometryChanged() {
    updateWidgetFromWindow(checked_cast<ZoomWindowWidget *>(sender()));
}

void ZoomWindowInstrument::at_windowWidget_doubleClicked() {
    ZoomWindowWidget *windowWidget = checked_cast<ZoomWindowWidget *>(sender());

    // TODO: #Elric does not belong here.
    QnMediaResourceWidget *zoomWidget = windowWidget->zoomWidget();
    if(zoomWidget)
        workbench()->setItem(Qn::ZoomedRole, zoomWidget->item());
}

void ZoomWindowInstrument::at_display_widgetChanged(Qn::ItemRole role) {
    if(role != Qn::ZoomedRole)
        return;

    if(m_zoomedWidget)
        updateOverlayVisibility(m_zoomedWidget.data());
    
    m_zoomedWidget = dynamic_cast<QnMediaResourceWidget *>(display()->widget(role));

    if(m_zoomedWidget)
        updateOverlayVisibility(m_zoomedWidget.data());
}

void ZoomWindowInstrument::at_display_zoomLinkAdded(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    QnMediaResourceWidget *mediaZoomTargetWidget = dynamic_cast<QnMediaResourceWidget *>(zoomTargetWidget);

    if(mediaWidget && mediaZoomTargetWidget)
        registerLink(mediaWidget, mediaZoomTargetWidget);
}

void ZoomWindowInstrument::at_display_zoomLinkAboutToBeRemoved(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    QnMediaResourceWidget *mediaZoomTargetWidget = dynamic_cast<QnMediaResourceWidget *>(zoomTargetWidget);

    if(mediaWidget && mediaZoomTargetWidget)
        unregisterLink(mediaWidget, mediaZoomTargetWidget);
}

void ZoomWindowInstrument::at_resizingStarted(QGraphicsView *, QGraphicsWidget *widget, ResizingInfo *info) {
    if(info->frameSection() != Qt::TitleBarArea)
        return;

    m_windowTarget = dynamic_cast<ZoomWindowWidget *>(widget);
}

void ZoomWindowInstrument::at_resizing(QGraphicsView *view, QGraphicsWidget *, ResizingInfo *info) {
    if(!windowTarget() || windowTarget() == m_storedWindowWidget.data())
        return;

    QnMediaResourceWidget *newTargetWidget = this->item<QnMediaResourceWidget>(view, info->mouseViewportPos());
    if(!newTargetWidget || newTargetWidget == windowTarget()->overlay()->target()) 
        return;
    
    QnAction *action = menu()->action(Qn::CreateZoomWindowAction);
    if(!action || action->checkCondition(action->scope(), QnActionParameters(newTargetWidget)) != Qn::EnabledAction)
        return;

    QnMediaResourceWidget *widget = windowTarget()->zoomWidget();
    QnMediaResourceWidget *oldTargetWidget = windowTarget()->overlay()->target();

    /* Preserve zoom window widget. */
    windowTarget()->hide();
    unregisterLink(widget, oldTargetWidget, false);

    if(m_storedWindowWidget)
        delete m_storedWindowWidget.data(); /* Just to feel safe that we don't leak memory. */
    m_storedWindowWidget = windowTarget();

    QRectF zoomRect = widget->zoomRect();
    QSizeF oldLayoutSize = oldTargetWidget->channelLayout()->size();
    QSizeF newLayoutSize = newTargetWidget->channelLayout()->size();
    if(oldLayoutSize != newLayoutSize) {
        QSizeF zoomSize = cwiseDiv(cwiseMul(zoomRect.size(), oldLayoutSize), newLayoutSize);
        zoomRect = movedInto(QRectF(zoomRect.topLeft(), zoomSize), QRectF(0.0, 0.0, 1.0, 1.0));
    }

    emit zoomTargetChanged(widget, zoomRect, newTargetWidget);
}

void ZoomWindowInstrument::at_resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *) {
    m_windowTarget.clear();
}


