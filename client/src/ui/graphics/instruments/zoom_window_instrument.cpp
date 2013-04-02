#include "zoom_window_instrument.h"

#include <utils/math/color_transformations.h>
#include <utils/common/checked_cast.h>

#include <ui/common/constrained_resizable.h>
#include <ui/style/globals.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace {
    const QColor zoomWindowColor = qnGlobals->zoomWindowColor();
    const QColor zoomFrameColor = toTransparent(zoomWindowColor, 0.5);

    const qreal zoomFrameWidth = qnGlobals->workbenchUnitSize() * 0.005; // TODO: #Elric move to settings;

} // anonymous namespace


// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
class ZoomWindowWidget: public GraphicsWidget, public ConstrainedResizable {
    typedef GraphicsWidget base_type;
public:
    ZoomWindowWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setWindowFrameMargins(zoomFrameWidth, zoomFrameWidth, zoomFrameWidth, zoomFrameWidth);
    }

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target) {
        m_target = target;
    }

protected:
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
    QWeakPointer<QnMediaResourceWidget> m_target;
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
        updateLayout();
    }

    void addZoomWindow(ZoomWindowWidget *zoomWindow) {
        zoomWindow->setParentItem(this);
        m_zoomWindows.push_back(zoomWindow);
    }

    void removeZoomWindow(ZoomWindowWidget *zoomWindow) {
        zoomWindow->setParentItem(NULL);
        m_zoomWindows.removeAll(zoomWindow);
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyCompare(oldSize, size()))
            updateLayout();
    }

    void updateLayout() {
        foreach(ZoomWindowWidget *zoomWindow, m_zoomWindows)
            if(QnMediaResourceWidget *widget = zoomWindow->target())
                zoomWindow->setGeometry(QnGeometry::transformed(widget->zoomWindow(), rect()));
    }

private:
    QList<ZoomWindowWidget *> m_zoomWindows;
};


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

void ZoomWindowInstrument::registerLink(QnMediaResourceWidget *zoomWindow, QnMediaResourceWidget *target) {
    
}

void ZoomWindowInstrument::unregisterLink(QnMediaResourceWidget *zoomWindow, QnMediaResourceWidget *target) {
    
}

void ZoomWindowInstrument::updateZoomType(QnMediaResourceWidget *widget, bool registerAsType) {
    ZoomData &data = m_dataByWidget[widget];
    bool isZoomWindow = qFuzzyCompare(widget->zoomWindow(), QRectF(0.0, 0.0, 1.0, 1.0));
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

void ZoomWindowInstrument::ensureOverlayWidget(QnMediaResourceWidget *widget) {
    ZoomData &data = m_dataByWidget[widget];

    ZoomOverlayWidget *overlay = data.overlayWidget;
    if(overlay)
        return;

    overlay = new ZoomOverlayWidget();
    overlay->setOpacity(0.0);
    data.overlayWidget = overlay;

    widget->addOverlayWidget(overlay, QnResourceWidget::Invisible, false, false);
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

