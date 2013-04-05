#ifndef QN_ZOOM_WINDOW_INSTRUMENT_H
#define QN_ZOOM_WINDOW_INSTRUMENT_H

#include "instrument.h"

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class ZoomOverlayWidget;
class ZoomWindowWidget;

class QnMediaResourceWidget;

class ZoomWindowInstrument: public Instrument, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Instrument base_type;

public:
    ZoomWindowInstrument(QObject *parent = NULL);
    virtual ~ZoomWindowInstrument();

signals:
    void zoomRectChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect);

protected:
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

private slots:
    void at_widget_aboutToBeDestroyed();
    void at_widget_zoomRectChanged();
    void at_windowWidget_geometryChanged();

    void at_display_zoomLinkAdded(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    void at_display_zoomLinkAboutToBeRemoved(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);

private:
    ZoomOverlayWidget *overlayWidget(QnMediaResourceWidget *widget) const;
    ZoomOverlayWidget *ensureOverlayWidget(QnMediaResourceWidget *widget);
    ZoomWindowWidget *windowWidget(QnMediaResourceWidget *widget) const;

    void registerWidget(QnMediaResourceWidget *widget);
    void unregisterWidget(QnMediaResourceWidget *widget);
    void registerLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget);
    void unregisterLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget);

    void updateWindowFromWidget(QnMediaResourceWidget *widget);
    void updateWidgetFromWindow(ZoomWindowWidget *windowWidget);

private:
    struct ZoomData {
        ZoomData(): overlayWidget(NULL), windowWidget(NULL) {}

        ZoomOverlayWidget *overlayWidget;
        ZoomWindowWidget *windowWidget;
    };

    QHash<QObject *, ZoomData> m_dataByWidget;
    QSet<QObject *> m_processingWidgets;
};

#endif // QN_ZOOM_WINDOW_INSTRUMENT_H
