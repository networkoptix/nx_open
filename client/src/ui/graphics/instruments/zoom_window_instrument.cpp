#include "zoom_window_instrument.h"

#include <ui/common/constrained_resizable.h>
#include <ui/style/globals.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace {
    const QColor zoomWindowColor = qnGlobals->zoomWindowColor();
    qreal zoomFrameWidth = qnGlobals->workbenchUnitSize() * 0.005; // TODO: #Elric move to settings;

} // anonymous namespace


// -------------------------------------------------------------------------- //
// ZoomSelectionWidget
// -------------------------------------------------------------------------- //
class ZoomSelectionWidget: public GraphicsWidget, public ConstrainedResizable {
    typedef GraphicsWidget base_type;
public:
    ZoomSelectionWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setWindowFrameMargins(zoomFrameWidth, zoomFrameWidth, zoomFrameWidth, zoomFrameWidth);
    }

    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        
    }

    virtual Qt::WindowFrameSection windowFrameSectionAt(const QPointF& pos) const override;

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
    }


};


// -------------------------------------------------------------------------- //
// ZoomWindowInstrument
// -------------------------------------------------------------------------- //
ZoomWindowInstrument::ZoomWindowInstrument(QObject *parent):
    base_type(Scene, makeSet(), parent)
{
}

ZoomWindowInstrument::~ZoomWindowInstrument() {
    return;
}

bool ZoomWindowInstrument::registeredNotify(QGraphicsItem *item) {
    if(!base_type::registeredNotify(item))
        return false;

    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item)) {
        return true;
    }

    return false;
}

void ZoomWindowInstrument::unregisteredNotify(QGraphicsItem *item) {
    base_type::unregisteredNotify(item);
}

