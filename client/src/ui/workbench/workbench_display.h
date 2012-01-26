#ifndef QN_WORKBENCH_MANAGER_H
#define QN_WORKBENCH_MANAGER_H

#include <QObject>
#include <QHash>
#include <core/resource/resource.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/animation/viewport_geometry_accessor.h>
#include <ui/common/scene_utility.h>
#include <utils/common/rect_set.h>
#include "workbench.h"
#include "recording/time_period.h"

class QGraphicsScene;
class QGraphicsView;

class InstrumentManager;
class BoundingInstrument;
class TransformListenerInstrument;
class ActivityListenerInstrument;
class ForwardingInstrument;
class AnimationInstrument;
class SelectionOverlayHackInstrument;

class QnAbstractRenderer;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnResourceWidget;
class QnResourceDisplay;
class ViewportAnimator;
class WidgetAnimator;
class QnCurtainAnimator;
class QnCurtainItem;
class QnGridItem;
class QnRenderWatchMixin;

class CLVideoCamera;
class CLCamDisplay;

/**
 * This class ties a workbench, a scene and a view together.
 * 
 * It presents some low-level functions for viewport and item manipulation.
 */
class QnWorkbenchDisplay: public QObject, protected AnimationTimerListener, protected SceneUtility {
    Q_OBJECT;
    Q_ENUMS(Layer);

public:
    /**
     * Layer of an item.
     */
    enum Layer {
        BACK_LAYER,                 /**< Back layer. */
        PINNED_LAYER,               /**< Layer for pinned items. */
        PINNED_RAISED_LAYER,        /**< Layer for pinned items that are raised. */
        UNPINNED_LAYER,             /**< Layer for unpinned items. */
        UNPINNED_RAISED_LAYER,      /**< Layer for unpinned items that are raised. */
        CURTAIN_LAYER,              /**< Layer for curtain that blacks out the background when an item is zoomed. */
        ZOOMED_LAYER,               /**< Layer for zoomed items. */
        FRONT_LAYER,                /**< Topmost layer for items. Items that are being dragged, resized or manipulated in any other way are to be placed here. */
        EFFECTS_LAYER,              /**< Layer for top-level effects. */
        UI_ELEMENTS_LAYER,          /**< Layer for ui elements, i.e. close button, navigation bar, etc... */
    };

    /**
     * Constructor.
     * 
     * \param workbench                 Workbench to display.
     * \param parent                    Parent object for this workbench display.
     */
    QnWorkbenchDisplay(QnWorkbench *workbench, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchDisplay();

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
     * \returns                         Activity listener instrument used by this workbench display.
     */
    ActivityListenerInstrument *activityListenerInstrument() const {
        return m_curtainActivityInstrument;
    }

    /**
     * \returns                         Paint forwarding instrument used by this workbench display.
     */
    ForwardingInstrument *paintForwardingInstrument() const {
        return m_paintForwardingInstrument;
    }

    /**
     * \returns                         Animation instrument used by this workbench display.
     */
    AnimationInstrument *animationInstrument() const {
        return m_animationInstrument;
    }

    SelectionOverlayHackInstrument *selectionOverlayHackInstrument() const {
        return m_selectionOverlayHackInstrument;
    }

    /**
     * Note that this function never returns NULL.
     * 
     * \returns                         Current workbench of this workbench display. 
     */
    QnWorkbench *workbench() const {
        return m_workbench;
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
    QGraphicsView *view() const {
        return m_view;
    }

    /**
     * \param view                      New view for this workbench display.
     */
    void setView(QGraphicsView *view);

    /**
     * \returns                         Grid item. 
     */
    QnGridItem *gridItem();

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

    QnResourceWidget *widget(const QnResourcePtr &resource) const;

    QnResourceWidget *widget(QnWorkbench::ItemRole role) const;

    QnResourceDisplay *display(QnWorkbenchItem *item) const;

    CLVideoCamera *camera(QnWorkbenchItem *item) const;

    CLCamDisplay *camDisplay(QnWorkbenchItem *item) const;

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

    Qn::MarginFlags marginFlags() const;

    void setMarginFlags(Qn::MarginFlags flags);

    void ensureVisible(QnWorkbenchItem *item);


    void bringToFront(const QList<QGraphicsItem *> &items);

    void bringToFront(QGraphicsItem *item);

    void bringToFront(QnWorkbenchItem *item);


    Layer layer(QGraphicsItem *item) const;

    void setLayer(QGraphicsItem *item, Layer layer);

    void setLayer(const QList<QGraphicsItem *> &items, Layer layer);

    qreal layerZValue(Layer layer) const;

    void synchronize(QnWorkbenchItem *item, bool animate = true);
    
    void synchronize(QnResourceWidget *widget, bool animate = true);


    QPoint mapViewportToGrid(const QPoint &viewportPoint) const;
    
    QPoint mapGlobalToGrid(const QPoint &globalPoint) const;

    QPointF mapViewportToGridF(const QPoint &viewportPoint) const;

    QPointF mapGlobalToGridF(const QPoint &globalPoint) const;

    QnRenderWatchMixin *renderWatcher() const;

public slots:
    void fitInView(bool animate = true);

signals:
    void viewportGrabbed();
    void viewportUngrabbed();

    void widgetAdded(QnResourceWidget *widget);
    void widgetAboutToBeRemoved(QnResourceWidget *widget);
    void widgetChanged(QnWorkbench::ItemRole role);
    
    //void playbackMaskChanged(const QnTimePeriodList&);
    void enableItemSync(bool value);

protected:
    virtual void tick(int deltaTime) override;
    
    WidgetAnimator *animator(QnResourceWidget *widget);

    void synchronizeGeometry(QnWorkbenchItem *item, bool animate);
    void synchronizeGeometry(QnResourceWidget *widget, bool animate);
    void synchronizeAllGeometries(bool animate);
    void synchronizeLayer(QnWorkbenchItem *item);
    void synchronizeLayer(QnResourceWidget *widget);
    void synchronizeSceneBounds();

    qreal layerFrontZValue(Layer layer) const;
    Layer synchronizedLayer(QnWorkbenchItem *item) const;

    void addItemInternal(QnWorkbenchItem *item);
    void removeItemInternal(QnWorkbenchItem *item, bool destroyWidget);

    void deinitSceneWorkbench();
    void initSceneWorkbench();
    void initWorkbench(QnWorkbench *workbench);
    void initBoundingInstrument();

    void changeItem(QnWorkbench::ItemRole role, QnWorkbenchItem *item);

protected slots:
    void synchronizeSceneBoundsExtension();
    void synchronizeRaisedGeometry();

    void at_scene_destroyed();

    void at_viewportAnimator_finished();

    void at_workbench_itemAdded(QnWorkbenchItem *item);
    void at_workbench_itemRemoved(QnWorkbenchItem *item);

    void at_workbench_aboutToBeDestroyed();
    void at_workbench_modeChanged();
    void at_workbench_itemChanged(QnWorkbench::ItemRole role);

    void at_item_geometryChanged();
    void at_item_geometryDeltaChanged();
    void at_item_rotationChanged();
    void at_item_flagsChanged();

    void at_activityStopped();
    void at_activityStarted();

    void at_curtained();
    void at_uncurtained();

    void at_widget_aboutToBeDestroyed();

    void at_view_destroyed();

    void at_mapper_originChanged();
    void at_mapper_cellSizeChanged();
    void at_mapper_spacingChanged();

private:
    /* Directly visible state */

    /** Current workbench. */
    QnWorkbench *m_workbench;

    /** Current scene. */
    QGraphicsScene *m_scene;

    /** Current view. */
    QGraphicsView *m_view;

    /** Render watcher. */
    QnRenderWatchMixin *m_renderWatcher;


    /* Internal state. */

    /** Resource to widget mapping. */
    QHash<QnResourcePtr, QnResourceWidget *> m_widgetByResource;

    /** Item to widget mapping. */
    QHash<QnWorkbenchItem *, QnResourceWidget *> m_widgetByItem;

    /** Renderer to widget mapping. */
    QHash<QnAbstractRenderer *, QnResourceWidget *> m_widgetByRenderer;

    /** Current front z displacement value. */
    qreal m_frontZ;

    /** Current items by role. */
    QnWorkbenchItem *m_itemByRole[QnWorkbench::ITEM_ROLE_COUNT];

    /** Current workbench mode. */
    QnWorkbench::Mode m_mode;

    /** Grid item. */
    QWeakPointer<QnGridItem> m_gridItem;


    /* Instruments. */

    /** Instrument manager owned by this workbench manager. */
    InstrumentManager *m_instrumentManager;

    /** Transformation listener instrument. */
    TransformListenerInstrument *m_transformListenerInstrument;

    /** Activity listener instrument for curtain item. */
    ActivityListenerInstrument *m_curtainActivityInstrument;

    /** Activity listener instrument for resource widgets. */
    ActivityListenerInstrument *m_widgetActivityInstrument;

    /** Bounding instrument. */
    BoundingInstrument *m_boundingInstrument;

    /** Paint forwarding instrument. */
    ForwardingInstrument *m_paintForwardingInstrument;

    /** Instrument that provides animation timer. */
    AnimationInstrument *m_animationInstrument;

    /** Selection overlay hack instrument. */
    SelectionOverlayHackInstrument *m_selectionOverlayHackInstrument;


    /* Animation-related stuff. */

    /** Viewport animator. */
    ViewportAnimator *m_viewportAnimator;

    /** Curtain item. */
    QWeakPointer<QnCurtainItem> m_curtainItem;

    /** Curtain animator. */
    QnCurtainAnimator *m_curtainAnimator;


    /* Helpers. */

    /** Stored dummy scene. */
    QGraphicsScene *m_dummyScene;
};

#endif // QN_WORKBENCH_MANAGER_H
