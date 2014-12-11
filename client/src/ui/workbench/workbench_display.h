#ifndef QN_WORKBENCH_MANAGER_H
#define QN_WORKBENCH_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <utils/common/uuid.h>
#include <QtOpenGL/QGLWidget>

#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <business/business_fwd.h>

#include <ui/common/geometry.h>
#include <ui/common/scene_transformations.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/graphics/view/graphics_view.h>

#include <client/client_globals.h>

#include "workbench_context_aware.h"

class QGraphicsScene;
class QGraphicsView;

class InstrumentManager;
class BoundingInstrument;
class TransformListenerInstrument;
class ActivityListenerInstrument;
class ForwardingInstrument;
class SignalingInstrument;
class SelectionOverlayHackInstrument;
class FocusListenerInstrument;

class QnAbstractRenderer;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnResourceWidget;
class QnResourceDisplay;
class ViewportAnimator;
class VariantAnimator;
class WidgetAnimator;
class QnCurtainAnimator;
class QnCurtainItem;
class QnGridItem;
class QnGridBackgroundItem;
class QnWorkbenchContext;
class QnWorkbenchStreamSynchronizer;
class QnToggle;
class QnThumbnailsLoader;
class QnThumbnail;
class QnGradientBackgroundPainter;

class QnClientVideoCamera;
class QnCamDisplay;

/**
 * This class ties a workbench, a scene and a view together.
 * 
 * It presents some low-level functions for viewport and item manipulation.
 */
class QnWorkbenchDisplay: public Connective<QObject>, public QnWorkbenchContextAware, protected QnGeometry, protected QnSceneTransformations {
    Q_OBJECT
    Q_PROPERTY(qreal widgetsFrameOpacity READ widgetsFrameOpacity WRITE setWidgetsFrameOpacity)

    typedef Connective<QObject> base_type;

public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent object for this workbench display.
     */
    QnWorkbenchDisplay(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchDisplay();

     /**
     * \returns                         Light mode of this workbench display.
     */
    Qn::LightModeFlags lightMode() const;

     /**
     * \param mode                      Light mode for the current display.
     *                                  Enables or disables certain visualization features
     *                                  to simplify and speed up painting.
     */
    void setLightMode(Qn::LightModeFlags mode);

    /**
     * \returns                         Instrument manager owned by this workbench display. 
     */
    InstrumentManager *instrumentManager() const {
        return m_instrumentManager;
    }

    /**
     * \returns                         Bounding instrument used by this workbench display.
     */
    BoundingInstrument *boundingInstrument() const {
        return m_boundingInstrument;
    }

    /**
     * \returns                         Transformation listener instrument used by this workbench display.
     */
    TransformListenerInstrument *transformationListenerInstrument() const {
        return m_transformListenerInstrument;
    }

    /**
     * \returns                         Activity listener instrument used by this workbench display to
     *                                  implement automatic curtaining.
     */
    ActivityListenerInstrument *activityListenerInstrument() const {
        return m_curtainActivityInstrument;
    }

    FocusListenerInstrument *focusListenerInstrument() const {
        return m_focusListenerInstrument;
    }

    /**
     * \returns                         Paint forwarding instrument used by this workbench display.
     */
    ForwardingInstrument *paintForwardingInstrument() const {
        return m_paintForwardingInstrument;
    }

    SelectionOverlayHackInstrument *selectionOverlayHackInstrument() const {
        return m_selectionOverlayHackInstrument;
    }

    SignalingInstrument *beforePaintInstrument() const {
        return m_beforePaintInstrument;
    }

    SignalingInstrument *afterPaintInstrument() const {
        return m_afterPaintInstrument;
    }


    /**
     * Note that this function never returns NULL.
     *
     * \returns                         Current scene of this workbench display.
     */
    QGraphicsScene *scene() const {
        return m_scene;
    }

    /**
     * Note that workbench manager does not take ownership of the supplied scene.
     *
     * \param scene                     New scene for this workbench display.
     *                                  If NULL is supplied, an empty scene
     *                                  owned by this workbench display is used.
     */
    void setScene(QGraphicsScene *scene);

    /**
     * \returns                         Current graphics view of this workbench display. 
     *                                  May be NULL.
     */
    QnGraphicsView *view() const {
        return m_view;
    }

    /**
     * \param view                      New view for this workbench display.
     */
    void setView(QnGraphicsView *view);

    /**
     * \returns                         Grid item. 
     */
    QnGridItem *gridItem() const;

    /**
     * \returns                         Curtain item (that is painted in black when a single widget is zoomed). 
     */
    QnCurtainItem* curtainItem() const;

    /**
     * \returns                         Curtain item animator. 
     */
    QnCurtainAnimator* curtainAnimator() const;

    /**
     * \returns                         Grid background item (E-Mapping).
     */
    QnGridBackgroundItem *gridBackgroundItem() const;

    /**
     * \param item                      Item to get widget for.
     * \returns                         Widget for the given item.
     */
    QnResourceWidget *widget(QnWorkbenchItem *item) const;

    /**
     * \param renderer                  Renderer to get widget for.
     * \returns                         Widget for the given renderer.
     */
    QnResourceWidget *widget(QnAbstractRenderer *renderer) const;

    QnResourceWidget *widget(Qn::ItemRole role) const;

    QnResourceWidget *widget(const QnUuid &uuid) const;

    QList<QnResourceWidget *> widgets(const QnResourcePtr &resource) const;

    QList<QnResourceWidget *> widgets() const;

    QnResourceWidget* activeWidget() const;

    QnResourceDisplay *display(QnWorkbenchItem *item) const;

    QnClientVideoCamera *camera(QnWorkbenchItem *item) const;

    QnCamDisplay *camDisplay(QnWorkbenchItem *item) const;

    /**
     * \param item                      Item to get enclosing geometry for.
     * \returns                         Given item's enclosing geometry in scene 
     *                                  coordinates as defined by the model.
     *                                  Note that actual geometry may differ because of
     *                                  aspect ration constraints.
     */
    QRectF itemEnclosingGeometry(QnWorkbenchItem *item) const;

    /**
     * \param item                      Item to get geometry for.
     * \param[out] enclosingGeometry    Item's enclosing geometry.
     * \returns                         Item's geometry in scene coordinates,
     *                                  taking aspect ratio constraints into account.
     *                                  Note that actual geometry of the item's widget
     *                                  may differ because of manual dragging / resizing / etc...
     */
    QRectF itemGeometry(QnWorkbenchItem *item, QRectF *enclosingGeometry = NULL) const;

    /**
     * \returns                         Bounding geometry of current layout.
     */
    QRectF layoutBoundingGeometry() const;

    QRectF fitInViewGeometry() const;

    /**
     * \returns                         Current viewport geometry, in scene coordinates.
     */
    QRectF viewportGeometry() const;

    /**
     * This function can be used in case the "actual" viewport differs from the 
     * "real" one. This can be the case if control panels are drawn on the scene.
     *
     * \param margins                   New viewport margins. 
     */
    void setViewportMargins(const QMargins &margins);

    QMargins viewportMargins() const;

    Qn::MarginFlags currentMarginFlags() const;

    Qn::MarginFlags zoomedMarginFlags() const;

    Qn::MarginFlags normalMarginFlags() const;

    void setZoomedMarginFlags(Qn::MarginFlags flags);

    void setNormalMarginFlags(Qn::MarginFlags flags);


    void bringToFront(const QList<QGraphicsItem *> &items);

    void bringToFront(QGraphicsItem *item);

    void bringToFront(QnWorkbenchItem *item);


    Qn::ItemLayer layer(QGraphicsItem *item) const;

    void setLayer(QGraphicsItem *item, Qn::ItemLayer layer);

    void setLayer(const QList<QGraphicsItem *> &items, Qn::ItemLayer layer);

    qreal layerZValue(Qn::ItemLayer layer) const;

    void synchronize(QnWorkbenchItem *item, bool animate = true);
    
    void synchronize(QnResourceWidget *widget, bool animate = true);


    QPoint mapViewportToGrid(const QPoint &viewportPoint) const;
    
    QPoint mapGlobalToGrid(const QPoint &globalPoint) const;

    QPointF mapViewportToGridF(const QPoint &viewportPoint) const;

    QPointF mapGlobalToGridF(const QPoint &globalPoint) const;

    /**
     * Status function to know if we are changing layout now.
     * \returns true if we are changing layout
     */
    bool isChangingLayout() const { return m_inChangeLayout; } // TODO: #Elric this is evil

    QnResourceWidget *zoomTargetWidget(QnResourceWidget *widget) const;

    void ensureRaisedConeItem(QnResourceWidget *widget);

    QGLWidget *newGlWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0) const;

public slots:
    void fitInView(bool animate = true);

signals:
    void viewportGrabbed();
    void viewportUngrabbed();

    void widgetAdded(QnResourceWidget *widget);
    void widgetAboutToBeRemoved(QnResourceWidget *widget);
    void widgetChanged(Qn::ItemRole role);

    void zoomLinkAdded(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    void zoomLinkAboutToBeRemoved(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);

    void resourceAdded(const QnResourcePtr &resource);
    void resourceAboutToBeRemoved(const QnResourcePtr &resource);

protected:
    WidgetAnimator *animator(QnResourceWidget *widget);

    void synchronizeGeometry(QnWorkbenchItem *item, bool animate);
    void synchronizeGeometry(QnResourceWidget *widget, bool animate);
    void synchronizeZoomRect(QnWorkbenchItem *item);
    void synchronizeZoomRect(QnResourceWidget *widget);
    void synchronizeAllGeometries(bool animate);
    void synchronizeLayer(QnWorkbenchItem *item);
    void synchronizeLayer(QnResourceWidget *widget);
    void synchronizeSceneBounds();

    void updateCurrentMarginFlags();

    void adjustGeometryLater(QnWorkbenchItem *item, bool animate = true);
    Q_SLOT void adjustGeometry(QnWorkbenchItem *item, bool animate = true);
    Q_SIGNAL void geometryAdjustmentRequested(QnWorkbenchItem *item, bool animate = true);

    qreal layerFrontZValue(Qn::ItemLayer layer) const;
    Qn::ItemLayer synchronizedLayer(QnResourceWidget *widget) const;
    Qn::ItemLayer shadowLayer(Qn::ItemLayer itemLayer) const;

    bool addItemInternal(QnWorkbenchItem *item, bool animate = true, bool startDisplay = true);
    bool removeItemInternal(QnWorkbenchItem *item, bool destroyWidget, bool destroyItem);

    bool addZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    bool removeZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    bool addZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    bool removeZoomLinkInternal(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget);
    bool removeZoomLinksInternal(QnWorkbenchItem *item);

    void deinitSceneView();
    void initSceneView();
    void initContext(QnWorkbenchContext *context);
    void initBoundingInstrument();

    void toggleBackgroundAnimation(bool enabled);

    qreal widgetsFrameOpacity() const;
    void setWidgetsFrameOpacity(qreal opacity);

    void setWidget(Qn::ItemRole role, QnResourceWidget *widget);

protected slots:
    void synchronizeSceneBoundsExtension();
    void synchronizeRaisedGeometry();
    void updateFrameWidths();

    void updateCurtainedCursor();
    void updateBackground(const QnLayoutResourcePtr &layout);

    /** Mark item on the scene selected as it was selected in the tree. */
    void updateSelectionFromTree();

    void at_scene_destroyed();
    void at_scene_selectionChanged();

    void at_viewportAnimator_finished();

    void at_workbench_itemChanged(Qn::ItemRole role, QnWorkbenchItem *item);
    void at_workbench_itemChanged(Qn::ItemRole role);
    void at_workbench_currentLayoutAboutToBeChanged();
    void at_workbench_currentLayoutChanged();

    void at_layout_itemAdded(QnWorkbenchItem *item);
    void at_layout_itemRemoved(QnWorkbenchItem *item);
    void at_layout_zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_layout_zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void at_layout_boundingRectChanged(const QRect &oldRect, const QRect &newRect);

    void at_context_permissionsChanged(const QnResourcePtr &resource);

    void at_item_geometryChanged();
    void at_item_geometryDeltaChanged();
    void at_item_zoomRectChanged();
    void at_item_rotationChanged();
    void at_item_dataChanged(int role);
    void at_item_flagChanged(Qn::ItemFlag flag, bool value);

    void at_curtainActivityInstrument_activityStopped();
    void at_curtainActivityInstrument_activityStarted();
    void at_widgetActivityInstrument_activityStopped();
    void at_widgetActivityInstrument_activityStarted();

    void at_widget_aboutToBeDestroyed();

    void at_view_destroyed();

    void at_mapper_originChanged();
    void at_mapper_cellSizeChanged();
    void at_mapper_spacingChanged();

    void at_loader_thumbnailLoaded(const QnThumbnail &thumbnail);

    void at_notificationsHandler_businessActionAdded(const QnAbstractBusinessActionPtr &businessAction);
    void at_notificationTimer_timeout(const QVariant &resource, const QVariant &type);
    void at_notificationTimer_timeout(const QnResourcePtr &resource, int type);

private:
    /* Directly visible state */

    /** Current scene. */
    QGraphicsScene *m_scene;

    /** Current view. */
    QnGraphicsView *m_view;

    /** Set of flags to simplify and speed up painting. */
    Qn::LightModeFlags m_lightMode;

    /** Zoomed state toggle. */
    QnToggle *m_zoomedToggle;


    /* Internal state. */

    QList<QnResourceWidget *> m_widgets;

    /** Item to widget mapping. */
    QHash<QnWorkbenchItem *, QnResourceWidget *> m_widgetByItem;

    /** Renderer to widget mapping. */
    QHash<QnAbstractRenderer *, QnResourceWidget *> m_widgetByRenderer; // TODO: #Elric not used anymore?

    /** Resource to widget mapping. */
    QHash<QnResourcePtr, QList<QnResourceWidget *> > m_widgetsByResource;

    /** Widget to zoom target widget mapping. */
    QHash<QnResourceWidget *, QnResourceWidget *> m_zoomTargetWidgetByWidget;

    /** Current front z displacement value. */
    qreal m_frontZ;

    /** Current items by role. */
    QnResourceWidget *m_widgetByRole[Qn::ItemRoleCount];

    /** Grid item. */
    QPointer<QnGridItem> m_gridItem;

    /** Grid background item. */
    QPointer<QnGridBackgroundItem> m_gridBackgroundItem;

    /* Background painter. */
    QPointer<QnGradientBackgroundPainter> m_backgroundPainter;

    /** Current frame opacity for widgets. */
    qreal m_frameOpacity;

    /** Whether frame widths need updating. */
    bool m_frameWidthsDirty;

    Qn::MarginFlags m_zoomedMarginFlags, m_normalMarginFlags;

    /** Whether we are changing layout now. */
    bool m_inChangeLayout;


    /* Instruments. */

    /** Instrument manager owned by this workbench manager. */
    InstrumentManager *m_instrumentManager;

    /** Transformation listener instrument. */
    TransformListenerInstrument *m_transformListenerInstrument;

    /** Activity listener instrument for curtain item. */
    ActivityListenerInstrument *m_curtainActivityInstrument;

    /** Activity listener instrument for resource widgets. */
    ActivityListenerInstrument *m_widgetActivityInstrument;

    /** Focus listener instrument. */
    FocusListenerInstrument *m_focusListenerInstrument;

    /** Bounding instrument. */
    BoundingInstrument *m_boundingInstrument;

    /** Paint forwarding instrument. */
    ForwardingInstrument *m_paintForwardingInstrument;

    /** Selection overlay hack instrument. */
    SelectionOverlayHackInstrument *m_selectionOverlayHackInstrument;

    SignalingInstrument *m_beforePaintInstrument;

    SignalingInstrument *m_afterPaintInstrument;


    /* Animation-related stuff. */

    /** Viewport animator. */
    ViewportAnimator *m_viewportAnimator;

    /** Curtain item. */
    QPointer<QnCurtainItem> m_curtainItem;

    /** Curtain animator. */
    QnCurtainAnimator *m_curtainAnimator;

    /** Frame opacity animator. */
    VariantAnimator *m_frameOpacityAnimator;

    QnThumbnailsLoader *m_loader;
};

#endif // QN_WORKBENCH_MANAGER_H
