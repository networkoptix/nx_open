#ifndef QN_ZOOM_WINDOW_INSTRUMENT_H
#define QN_ZOOM_WINDOW_INSTRUMENT_H

#include "instrument.h"

#include <core/resource/resource_fwd.h>

class ZoomOverlayWidget;
class ZoomWindowWidget;

class QnMediaResourceWidget;

class ZoomWindowInstrument: public Instrument {
    Q_OBJECT
    typedef Instrument base_type;

public:
    ZoomWindowInstrument(QObject *parent = NULL);
    virtual ~ZoomWindowInstrument();

protected:
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

private slots:
    void at_widget_zoomWindowChanged();
    void at_widget_aboutToBeDestroyed();
    void at_zoomWindow_geometryChanged();

private:
    ZoomOverlayWidget *overlayWidget(QnMediaResourceWidget *widget) const;
    ZoomOverlayWidget *ensureOverlayWidget(QnMediaResourceWidget *widget);
    ZoomWindowWidget *windowWidget(QnMediaResourceWidget *widget) const;

    void updateZoomType(QnMediaResourceWidget *widget, bool registerAsType = true);
    void updateWindowFromWidget(QnMediaResourceWidget *widget);
    void updateWidgetFromWindow(ZoomWindowWidget *windowWidget);

    void registerWidget(QnMediaResourceWidget *widget);
    void unregisterWidget(QnMediaResourceWidget *widget);

    void registerWidgetAs(QnMediaResourceWidget *widget, bool asZoomWindow);
    void unregisterWidgetAs(QnMediaResourceWidget *widget, bool asZoomWindow);

    void registerLink(QnMediaResourceWidget *sourceWidget, QnMediaResourceWidget *targetWidget);
    void unregisterLink(QnMediaResourceWidget *sourceWidget, QnMediaResourceWidget *targetWidget);

private:
    struct ZoomData {
        ZoomData(): isZoomWindow(false), overlayWidget(NULL), windowWidget(NULL) {}

        bool isZoomWindow;
        ZoomOverlayWidget *overlayWidget;
        ZoomWindowWidget *windowWidget;
    };

    struct ZoomWidgets {
        ZoomWidgets(): target(NULL) {}

        QnMediaResourceWidget *target;
        QList<QnMediaResourceWidget *> normal;
        QList<QnMediaResourceWidget *> zoom;
    };

    QHash<QObject *, ZoomData> m_dataByWidget;
    QHash<QnMediaResourcePtr, ZoomWidgets> m_widgetsByResource;
};

#endif // QN_ZOOM_WINDOW_INSTRUMENT_H
