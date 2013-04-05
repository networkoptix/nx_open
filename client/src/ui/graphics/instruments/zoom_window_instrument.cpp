#include "zoom_window_instrument.h"

#include <utils/math/color_transformations.h>
#include <utils/common/checked_cast.h>

#include <ui/common/constrained_geometrically.h>
#include <ui/common/constrained_resizable.h>
#include <ui/style/globals.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

namespace {
    const QColor zoomWindowColor = qnGlobals->zoomWindowColor();
    const QColor zoomFrameColor = toTransparent(zoomWindowColor, 0.5);

    const qreal zoomFrameWidth = qnGlobals->workbenchUnitSize() * 0.005; // TODO: #Elric move to settings;

    const qreal zoomWindowMinSize = 0.1;

} // anonymous namespace

class ZoomOverlayWidget;

// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
class ZoomWindowWidget: public Instrumented<GraphicsWidget>, public ConstrainedGeometrically {
    typedef Instrumented<GraphicsWidget> base_type;
public:
    ZoomWindowWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setWindowFrameMargins(zoomFrameWidth, zoomFrameWidth, zoomFrameWidth, zoomFrameWidth);

        setAcceptedMouseButtons(Qt::LeftButton);
        setHandlingFlag(ItemHandlesResizing, true);
        setHandlingFlag(ItemHandlesMovement, true);

        setWindowFlags(this->windowFlags() | Qt::Window);
        setFlag(ItemIsPanel, false); /* See comment in workbench_display.cpp. */
        
        setFlag(ItemIsMovable, true);
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

protected:
    virtual QRectF constrainedGeometry(const QRectF &geometry, Qn::Corner pinCorner) const override;

    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        qreal l, t, r, b;
        getWindowFrameMargins(&l, &t, &r, &b);

        QSizeF size = this->size();
        qreal w = size.width();
        qreal h = size.height();
        QColor color = zoomFrameColor;

        painter->fillRect(QRectF(-l,      -t,      w + l + r,   t), color);
        painter->fillRect(QRectF(-l,      h,       w + l + r,   b), color);
        painter->fillRect(QRectF(-l,      0,       l,           h),  color);
        painter->fillRect(QRectF(w,       0,       r,           h),  color);
    }

    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override {
        Qn::WindowFrameSections result = base_type::windowFrameSectionsAt(region);

        if(result & Qn::SideSections) {
            result &= ~Qn::SideSections;
            result |= Qn::TitleBarArea;
        } else if(result == Qn::NoSection) {
            result = Qn::TitleBarArea;
        }

        return result;
    }

private:
    QWeakPointer<ZoomOverlayWidget> m_overlay;
    QWeakPointer<QnMediaResourceWidget> m_zoomWidget;
};


// -------------------------------------------------------------------------- //
// ZoomOverlayWidget
// -------------------------------------------------------------------------- //
class ZoomOverlayWidget: public QGraphicsWidget {
    typedef QGraphicsWidget base_type;
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
        widget->setGeometry(QnGeometry::transformed(rect(), m_rectByWidget.value(widget)));
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

QRectF ZoomWindowWidget::constrainedGeometry(const QRectF &geometry, Qn::Corner pinCorner) const {
    ZoomOverlayWidget *overlayWidget = this->overlay();
    QRectF result = geometry;

    /* Size constraints go first. */
    QSizeF maxSize = geometry.size();
    if(overlayWidget)
        maxSize = QnGeometry::cwiseMax(QnGeometry::cwiseMin(maxSize, overlayWidget->size()), overlayWidget->size() * zoomWindowMinSize);
    result = ConstrainedResizable::constrainedGeometry(geometry, pinCorner, QnGeometry::expanded(QnGeometry::aspectRatio(size()), maxSize, Qt::KeepAspectRatio));

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
            result = ConstrainedResizable::constrainedGeometry(result, pinCorner, result.size() * scaleFactor);
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
    base_type(Item, makeSet(), parent),
    QnWorkbenchContextAware(parent)
{
    connect(display(), SIGNAL(zoomLinkAdded(QnResourceWidget *, QnResourceWidget *)), this, SLOT(at_display_zoomLinkAdded(QnResourceWidget *, QnResourceWidget *)));
    connect(display(), SIGNAL(zoomLinkAboutToBeRemoved(QnResourceWidget *, QnResourceWidget *)), this, SLOT(at_display_zoomLinkAboutToBeRemoved(QnResourceWidget *, QnResourceWidget *)));
}

ZoomWindowInstrument::~ZoomWindowInstrument() {
    return;
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

    widget->addOverlayWidget(overlayWidget, QnResourceWidget::AutoVisible, false, false);
    return overlayWidget;
}

ZoomWindowWidget *ZoomWindowInstrument::windowWidget(QnMediaResourceWidget *widget) const {
    return m_dataByWidget[widget].windowWidget;
}

void ZoomWindowInstrument::registerWidget(QnMediaResourceWidget *widget) {
    connect(widget, SIGNAL(zoomRectChanged()), this, SLOT(at_widget_zoomRectChanged()));
    connect(widget, SIGNAL(aboutToBeDestroyed()), this, SLOT(at_widget_aboutToBeDestroyed()));
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

    ZoomWindowWidget *windowWidget = new ZoomWindowWidget();
    windowWidget->setZoomWidget(widget);
    overlayWidget->addWidget(windowWidget);
    data.windowWidget = windowWidget;
    connect(windowWidget, SIGNAL(geometryChanged()), this, SLOT(at_windowWidget_geometryChanged()));

    updateWindowFromWidget(widget);
}

void ZoomWindowInstrument::unregisterLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget) {
    ZoomData &data = m_dataByWidget[widget];

    delete data.windowWidget;
    data.windowWidget = NULL;
}

void ZoomWindowInstrument::updateWindowFromWidget(QnMediaResourceWidget *widget) {
    if(m_processingWidgets.contains(widget))
        return;
    m_processingWidgets.insert(widget);

    ZoomWindowWidget *windowWidget = this->windowWidget(widget);
    ZoomOverlayWidget *overlayWidget = windowWidget ? windowWidget->overlay() : NULL;

    if(windowWidget && overlayWidget)
        overlayWidget->setWidgetRect(windowWidget, widget->zoomRect());

    m_processingWidgets.remove(widget);
}

void ZoomWindowInstrument::updateWidgetFromWindow(ZoomWindowWidget *windowWidget) {
    ZoomOverlayWidget *overlayWidget = windowWidget->overlay();
    QnMediaResourceWidget *zoomWidget = windowWidget->zoomWidget();

    if(m_processingWidgets.contains(zoomWidget))
        return;
    m_processingWidgets.insert(zoomWidget);

    if(overlayWidget && zoomWidget)
        emit zoomRectChanged(zoomWidget, QnGeometry::cwiseDiv(windowWidget->geometry(), overlayWidget->size()));

    m_processingWidgets.remove(zoomWidget);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
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

void ZoomWindowInstrument::at_widget_aboutToBeDestroyed() {
    unregisterWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_zoomRectChanged() {
    updateWindowFromWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_windowWidget_geometryChanged() {
    updateWidgetFromWindow(checked_cast<ZoomWindowWidget *>(sender()));
}
