#ifndef QN_ZOOM_WINDOW_INSTRUMENT_H
#define QN_ZOOM_WINDOW_INSTRUMENT_H

#include "drag_processing_instrument.h"

#include <client/client_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class FixedArSelectionItem;
class ZoomOverlayWidget;
class ZoomWindowWidget;
class ResizingInfo;

class QnMediaResourceWidget;

class ZoomWindowInstrument: public DragProcessingInstrument, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef DragProcessingInstrument base_type;

public:
    ZoomWindowInstrument(QObject *parent = NULL);
    virtual ~ZoomWindowInstrument();

signals:
    void zoomRectCreated(QnMediaResourceWidget *widget, const QColor &color, const QRectF &zoomRect);
    void zoomRectChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect);
    void zoomTargetChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect, QnMediaResourceWidget *zoomTargetWidget);

    void zoomWindowProcessStarted(QnMediaResourceWidget *widget);
    void zoomWindowStarted(QnMediaResourceWidget *widget);
    void zoomWindowFinished(QnMediaResourceWidget *widget);
    void zoomWindowProcessFinished(QnMediaResourceWidget *widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private slots:
    void at_widget_aboutToBeDestroyed();
    void at_widget_zoomRectChanged();
    void at_widget_frameColorChanged();
    void at_widget_optionsChanged();
    void at_windowWidget_geometryChanged();
    void at_windowWidget_doubleClicked();

    void at_display_widgetChanged(Qn::ItemRole role);
    void at_display_zoomLinkAdded(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    void at_display_zoomLinkAboutToBeRemoved(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);

    void at_resizingStarted(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void at_resizing(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);
    void at_resizingFinished(QGraphicsView *view, QGraphicsWidget *widget, ResizingInfo *info);

private:
    QColor nextZoomWindowColor() const;

    ZoomOverlayWidget *overlayWidget(QnMediaResourceWidget *widget) const;
    ZoomOverlayWidget *ensureOverlayWidget(QnMediaResourceWidget *widget);
    ZoomWindowWidget *windowWidget(QnMediaResourceWidget *widget) const;

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    ZoomWindowWidget *windowTarget() const {
        return m_windowTarget.data();
    }

    FixedArSelectionItem *selectionItem() const {
        return m_selectionItem.data();
    }
    void ensureSelectionItem();

    void registerWidget(QnMediaResourceWidget *widget);
    void unregisterWidget(QnMediaResourceWidget *widget);
    void registerLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget);
    void unregisterLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget, bool deleteWindowWidget = true);

    void updateOverlayVisibility(QnMediaResourceWidget *widget);
    void updateWindowFromWidget(QnMediaResourceWidget *widget);
    void updateWidgetFromWindow(ZoomWindowWidget *windowWidget);

private:
    struct ZoomData {
        ZoomData(): overlayWidget(NULL), windowWidget(NULL) {}

        ZoomOverlayWidget *overlayWidget;
        ZoomWindowWidget *windowWidget;
    };

    bool m_zoomWindowStartedEmitted;
    QVector<QColor> m_zoomWindowColors;
    QWeakPointer<FixedArSelectionItem> m_selectionItem;
    QWeakPointer<QWidget> m_viewport;
    QColor m_zoomWindowColor;
    QWeakPointer<QnMediaResourceWidget> m_target;
    QHash<QObject *, ZoomData> m_dataByWidget;
    QSet<QObject *> m_processingWidgets;
    QWeakPointer<QnMediaResourceWidget> m_zoomedWidget;
    QWeakPointer<ZoomWindowWidget> m_windowTarget;
    QWeakPointer<ZoomWindowWidget> m_storedWindowWidget;
};

#endif // QN_ZOOM_WINDOW_INSTRUMENT_H
