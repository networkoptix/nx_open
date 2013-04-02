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

class ZoomOverlayWidget;

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

    ZoomOverlayWidget *overlay() const {
        return m_overlay.data();
    }

    void setOverlay(ZoomOverlayWidget *overlay) {
        m_overlay = overlay;
    }

protected:
    virtual QSizeF constrainedSize(const QSizeF constraint) const override {
        return constraint;
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

    void addWidget(ZoomWindowWidget *widget) {
        widget->setParentItem(this);
        m_rectByWidget.insert(widget, QRectF(0.0, 0.0, 1.0, 1.0));
    }

    void removeWidget(ZoomWindowWidget *widget) {
        if(widget->parentItem() == this)
            widget->setParentItem(NULL);
        m_rectByWidget.remove(widget);
    }
    
    void setWidgetRect(ZoomWindowWidget *widget, const QRectF &rect) {
        if(!m_rectByWidget.contains(widget))
            return;

        m_rectByWidget[widget] = rect;
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

    void updateLayout() {
        for(QHash<ZoomWindowWidget *, QRectF>::const_iterator pos = m_rectByWidget.begin(); pos != m_rectByWidget.end(); pos++)
            pos.key()->setGeometry(QnGeometry::transformed(pos.value(), rect()));
    }

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        if(change == ItemChildRemovedChange) {
            ZoomWindowWidget *widget = static_cast<ZoomWindowWidget *>(value.value<QGraphicsItem *>());
            m_rectByWidget.remove(widget); /* Cannot call removeWidget here as it will change widget's parent. */
        }

        return base_type::itemChange(change, value);
    }

private:
    QHash<ZoomWindowWidget *, QRectF> m_rectByWidget;
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

void ZoomWindowInstrument::registerLink(QnMediaResourceWidget *sourceWidget, QnMediaResourceWidget *targetWidget) {
    ZoomData &data = m_dataByWidget[sourceWidget];

    ZoomOverlayWidget *overlayWidget = ensureOverlayWidget(targetWidget);

    ZoomWindowWidget *windowWidget = new ZoomWindowWidget();
    overlayWidget->addWidget(windowWidget);
    updateWindowFromWidget(sourceWidget);
}

void ZoomWindowInstrument::unregisterLink(QnMediaResourceWidget *sourceWidget, QnMediaResourceWidget *targetWidget) {
    
}

void ZoomWindowInstrument::updateWindowFromWidget(QnMediaResourceWidget *widget) {
    ZoomWindowWidget *windowWidget = this->windowWidget(widget);

    windowWidget->overlay()->setWidgetRect(windowWidget, widget->zoomWindow());
}

void ZoomWindowInstrument::updateWidgetFromWindow(ZoomWindowWidget *windowWidget) {
    
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

ZoomOverlayWidget *ZoomWindowInstrument::ensureOverlayWidget(QnMediaResourceWidget *widget) {
    ZoomData &data = m_dataByWidget[widget];

    ZoomOverlayWidget *overlayWidget = data.overlayWidget;
    if(overlayWidget)
        return overlayWidget;

    overlayWidget = new ZoomOverlayWidget();
    overlayWidget->setOpacity(0.0);
    data.overlayWidget = overlayWidget;

    widget->addOverlayWidget(overlayWidget, QnResourceWidget::Invisible, false, false);
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

