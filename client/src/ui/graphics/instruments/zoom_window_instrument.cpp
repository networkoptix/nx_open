#include "zoom_window_instrument.h"

#include <utils/math/color_transformations.h>
#include <utils/common/checked_cast.h>

#include <ui/common/constrained_resizable.h>
#include <ui/style/globals.h>
#include <ui/graphics/instruments/instrumented.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace {
    const QColor zoomWindowColor = qnGlobals->zoomWindowColor();
    const QColor zoomFrameColor = toTransparent(zoomWindowColor, 0.5);

    const qreal zoomFrameWidth = qnGlobals->workbenchUnitSize() * 0.005; // TODO: #Elric move to settings;

} // anonymous namespace

class ZoomOverlayWidget;

// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
class ZoomWindowWidget: public Instrumented<GraphicsWidget>, public ConstrainedResizable {
    typedef Instrumented<GraphicsWidget> base_type;
public:
    ZoomWindowWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_aspectRatio(-1.0)
    {
        setWindowFrameMargins(zoomFrameWidth, zoomFrameWidth, zoomFrameWidth, zoomFrameWidth);

        setWindowFlags(this->windowFlags() | Qt::Window);
        setFlag(ItemIsPanel, false); /* See comment in workbench_display.cpp. */
        
        setFlag(ItemIsMovable, true);
    }

    virtual ~ZoomWindowWidget();

    qreal aspectRatio() const {
        return m_aspectRatio;
    }

    void setAspectRatio(qreal aspectRatio) {
        m_aspectRatio = aspectRatio;
    }

    ZoomOverlayWidget *overlay() const {
        return m_overlay.data();
    }

    void setOverlay(ZoomOverlayWidget *overlay) {
        m_overlay = overlay;
    }

protected:
    virtual QSizeF constrainedSize(const QSizeF constraint) const override {
        if(m_aspectRatio < 0.0)
            return constraint;
        else
            return QnGeometry::expanded(m_aspectRatio, constraint, Qt::KeepAspectRatio);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        painter->setPen(Qt::green);
        painter->drawRect(rect());
    }

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
        }

        return result;
    }

private:
    QWeakPointer<ZoomOverlayWidget> m_overlay;
    qreal m_aspectRatio;
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
        
        updateLayout();
        updateAspectRatio();
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

        updateLayout();
    }

    QRectF widgetRect(ZoomWindowWidget *widget) const {
        return m_rectByWidget.value(widget, QRectF());
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();
        qreal oldAspectRatio = QnGeometry::aspectRatio(oldSize);

        base_type::setGeometry(rect);

        if(!qFuzzyCompare(oldSize, size()))
            updateLayout();

        if(!qFuzzyCompare(oldAspectRatio, QnGeometry::aspectRatio(size())))
            updateAspectRatio();
    }

    void updateLayout() {
        for(QHash<ZoomWindowWidget *, QRectF>::const_iterator pos = m_rectByWidget.begin(); pos != m_rectByWidget.end(); pos++)
            pos.key()->setGeometry(QnGeometry::transformed(rect(), pos.value()));
    }

    void updateAspectRatio() {
        qreal aspectRatio = QnGeometry::aspectRatio(size());
        for(QHash<ZoomWindowWidget *, QRectF>::const_iterator pos = m_rectByWidget.begin(); pos != m_rectByWidget.end(); pos++)
            pos.key()->setAspectRatio(aspectRatio);
    }

private:
    QHash<ZoomWindowWidget *, QRectF> m_rectByWidget;
    QWeakPointer<QnMediaResourceWidget> m_target;
};

ZoomWindowWidget::~ZoomWindowWidget() {
    if(overlay())
        overlay()->removeWidget(this);
}



// -------------------------------------------------------------------------- //
// ZoomWindowInstrument
// -------------------------------------------------------------------------- //
ZoomWindowInstrument::ZoomWindowInstrument(QObject *parent):
    base_type(Item, makeSet(), parent)
{
}

ZoomWindowInstrument::~ZoomWindowInstrument() {
    return;
}

void ZoomWindowInstrument::registerWidget(QnMediaResourceWidget *widget) {
    connect(widget, SIGNAL(zoomWindowChanged()), this, SLOT(at_widget_zoomWindowChanged()));
    connect(widget, SIGNAL(aboutToBeDestroyed()), this, SLOT(at_widget_aboutToBeDestroyed()));
    
    updateZoomType(widget, false);
    registerWidgetAs(widget, m_dataByWidget[widget].isZoomWindow);
}

void ZoomWindowInstrument::unregisterWidget(QnMediaResourceWidget *widget) {
    if(!m_dataByWidget.contains(widget))
        return; /* Double unregistration is possible and is OK. */

    disconnect(widget, NULL, this, NULL);

    unregisterWidgetAs(widget, m_dataByWidget[widget].isZoomWindow);

    m_dataByWidget.remove(widget);
}

void ZoomWindowInstrument::registerWidgetAs(QnMediaResourceWidget *widget, bool asZoomWindow) {
    ZoomWidgets &widgets = m_widgetsByResource[widget->resource()];

    if(asZoomWindow) {
        widgets.zoom.push_back(widget);
        if(widgets.target)
            registerLink(widget, widgets.target);
    } else {
        widgets.normal.push_back(widget);
        if(widgets.normal.size() == 1) {
            widgets.target = widget;
            foreach(QnMediaResourceWidget *zoomWindow, widgets.zoom)
                registerLink(zoomWindow, widgets.target);
        }
    }
}

void ZoomWindowInstrument::unregisterWidgetAs(QnMediaResourceWidget *widget, bool asZoomWindow) {
    ZoomWidgets &widgets = m_widgetsByResource[widget->resource()];

    if(asZoomWindow) {
        widgets.zoom.removeOne(widget);
        if(widgets.target)
            unregisterLink(widget, widgets.target);
    } else {
        widgets.normal.removeOne(widget);
        if(widgets.target == widget) {
            foreach(QnMediaResourceWidget *zoomWindow, widgets.zoom)
                unregisterLink(zoomWindow, widgets.target);
            if(widgets.normal.isEmpty()) {
                widgets.target = NULL;
            } else {
                widgets.target = widgets.normal[0];
                foreach(QnMediaResourceWidget *zoomWindow, widgets.zoom)
                    registerLink(zoomWindow, widgets.target);
            }
        }
    }
}

void ZoomWindowInstrument::registerLink(QnMediaResourceWidget *sourceWidget, QnMediaResourceWidget *targetWidget) {
    ZoomData &data = m_dataByWidget[sourceWidget];

    ZoomOverlayWidget *overlayWidget = ensureOverlayWidget(targetWidget);
    
    ZoomWindowWidget *windowWidget = new ZoomWindowWidget();
    overlayWidget->addWidget(windowWidget);
    data.windowWidget = windowWidget;
    connect(windowWidget, SIGNAL(geometryChanged()), this, SLOT(at_zoomWindow_geometryChanged()));

    updateWindowFromWidget(sourceWidget);
}

void ZoomWindowInstrument::unregisterLink(QnMediaResourceWidget *sourceWidget, QnMediaResourceWidget *targetWidget) {
    ZoomData &data = m_dataByWidget[sourceWidget];

    delete data.windowWidget;
    data.windowWidget = NULL;
}

void ZoomWindowInstrument::updateWindowFromWidget(QnMediaResourceWidget *widget) {
    ZoomWindowWidget *windowWidget = this->windowWidget(widget);

    windowWidget->overlay()->setWidgetRect(windowWidget, widget->zoomWindow());
}

void ZoomWindowInstrument::updateWidgetFromWindow(ZoomWindowWidget *windowWidget) {
    ZoomOverlayWidget *overlayWidget = windowWidget->overlay();
    QnMediaResourceWidget *mediaWidget = overlayWidget->target();

    mediaWidget->setZoomWindow(QnGeometry::cwiseDiv(windowWidget->rect(), overlayWidget->size()));
}

void ZoomWindowInstrument::updateZoomType(QnMediaResourceWidget *widget, bool registerAsType) {
    ZoomData &data = m_dataByWidget[widget];
    bool isZoomWindow = !qFuzzyCompare(widget->zoomWindow(), QRectF(0.0, 0.0, 1.0, 1.0));
    if(data.isZoomWindow == isZoomWindow)
        return;

    if(registerAsType)
        unregisterWidgetAs(widget, data.isZoomWindow);

    data.isZoomWindow = isZoomWindow;

    if(registerAsType)
        registerWidgetAs(widget, data.isZoomWindow);
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

void ZoomWindowInstrument::at_widget_zoomWindowChanged() {
    updateZoomType(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_aboutToBeDestroyed() {
    unregisterWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_zoomWindow_geometryChanged() {
    updateWidgetFromWindow(checked_cast<ZoomWindowWidget *>(sender()));
}
