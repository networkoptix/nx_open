#ifndef QN_WORKBENCH_MANAGER_H
#define QN_WORKBENCH_MANAGER_H

#include <QObject>
#include <ui/animation/animation_timer.h>
#include <ui/common/scene_utility.h>
#include <utils/common/rect_set.h>
#include "workbench.h"

class QGraphicsScene;
class QGraphicsView;

class InstrumentManager;
class BoundingInstrument;
class TransformListenerInstrument;
class ActivityListenerInstrument;
class ForwardingInstrument;

class CLAbstractRenderer;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnResourceWidget;
class QnResourceDisplay;
class QnViewportAnimator;
class QnWidgetAnimator;
class QnCurtainAnimator;
class QnCurtainItem;

class CLVideoCamera;
class CLCamDisplay;

/**
 * This class ties a workbench, a scene and a view together.
 * 
 * It presents some low-level functions for viewport and item manipulation.
 */
class QnWorkbenchDisplay: public QObject, protected AnimationTimerListener, protected QnSceneUtility {
    Q_OBJECT;
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
     * \param workbench                 Workbench to manage.
     * \param parent                    Parent object for this workbench manager.
     */
    QnWorkbenchDisplay(QnWorkbench *workbench, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchDisplay();

    /**
     * \returns                         Instrument manager owned by this workbench manager. 
     */
    InstrumentManager *instrumentManager() const {
        return m_instrumentManager;
    }

    /**
     * \returns                         Bounding instrument used by this workbench manager.
     */
    BoundingInstrument *boundingInstrument() const {
        return m_boundingInstrument;
    }

    /**
     * \returns                         Transformation listener instrument used by this workbench manager.
     */
    TransformListenerInstrument *transformationListenerInstrument() const {
        return m_transformListenerInstrument;
    }

    /**
     * \returns                         Activity listener instrument used by this workbench manager.
     */
    ActivityListenerInstrument *activityListenerInstrument() const {
        return m_activityListenerInstrument;
    }

    /**
     * \returns                         Paint forwarding instrument used by this workbench manager.
     */
    ForwardingInstrument *paintForwardingInstrument() const {
        return m_paintForwardingInstrument;
    }

    /**
     * Note that this function never returns NULL.
     * 
     * \returns                         Current workbench of this workbench manager. 
     */
    QnWorkbench *workbench() const {
        return m_workbench;
    }

    /**
     * Note that this function never returns NULL.
     *
     * \returns                         Current scene of this workbench manager.
     */
    QGraphicsScene *scene() const {
        return m_scene;
    }

    /**
     * Note that workbench manager does not take ownership of the supplied scene.
     *
     * \param scene                     New scene for this workbench manager.
     *                                  If NULL is supplied, an empty scene
     *                                  owned by this workbench manager is used.
     */
    void setScene(QGraphicsScene *scene);

    /**
     * \returns                         Current graphics view of this workbench manager. 
     *                                  May be NULL.
     */
    QGraphicsView *view() const {
        return m_view;
    }

    /**
     * \param view                      New view for this workbench manager.
     */
    void setView(QGraphicsView *view);

    /**
     * \param item                      Item to get widget for.
     * \returns                         Widget for the given item.
     */
    QnResourceWidget *widget(QnWorkbenchItem *item) const;

    /**
     * \param renderer                  Renderer to get widget for.
     * \returns                         Widget for the given renderer.
     */
    QnResourceWidget *widget(CLAbstractRenderer *renderer) const;

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

    /**
     * \returns                         Current viewport geometry, in scene coordinates.
     */
    QRectF viewportGeometry() const;


    void fitInView();

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



signals:
    void viewportGrabbed();
    void viewportUngrabbed();

    void widgetAdded(QnResourceWidget *widget);
    void widgetAboutToBeRemoved(QnResourceWidget *widget);

protected:
    virtual void tick(int deltaTime) override;
    
    QnWidgetAnimator *animator(QnResourceWidget *widget);

    void synchronizeGeometry(QnWorkbenchItem *item, bool animate);
    void synchronizeGeometry(QnResourceWidget *widget, bool animate);
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

protected slots:
    void at_viewport_animationFinished();

    void at_workbench_itemAdded(QnWorkbenchItem *item);
    void at_workbench_itemAboutToBeRemoved(QnWorkbenchItem *item);
    
    void at_workbench_aboutToBeDestroyed();
    void at_workbench_modeChanged();
    void at_workbench_raisedItemChanged();
    void at_workbench_zoomedItemChanged();
    void at_workbench_focusedItemChanged();

    void at_item_geometryChanged();
    void at_item_geometryDeltaChanged();
    void at_item_rotationChanged();
    void at_item_flagsChanged();

    void at_viewport_transformationChanged();

    void at_activityStopped();
    void at_activityStarted();

    void at_curtained();
    void at_uncurtained();

    void at_widget_aboutToBeDestroyed();
    void at_scene_destroyed();
    void at_view_destroyed();

private:
    /* Directly visible state */

    /** Current workbench. */
    QnWorkbench *m_workbench;

    /** Current scene. */
    QGraphicsScene *m_scene;

    /** Current view. */
    QGraphicsView *m_view;


    /* Internal state. */

    /** Item to widget mapping. */
    QHash<QnWorkbenchItem *, QnResourceWidget *> m_widgetByItem;

    /** Renderer to widget mapping. */
    QHash<CLAbstractRenderer *, QnResourceWidget *> m_widgetByRenderer;

    /** Current front z displacement value. */
    qreal m_frontZ;

    /** Currently raised item. */
    QnWorkbenchItem *m_raisedItem;

    /** Currently zoomed item. */
    QnWorkbenchItem *m_zoomedItem;

    /** Currently focused item. */
    QnWorkbenchItem *m_focusedItem;

    /** Current workbench mode. */
    QnWorkbench::Mode m_mode;


    /* Instruments. */

    /** Instrument manager owned by this workbench manager. */
    InstrumentManager *m_instrumentManager;

    /** Transformation listener instrument. */
    TransformListenerInstrument *m_transformListenerInstrument;

    /** Activity listener instrument. */
    ActivityListenerInstrument *m_activityListenerInstrument;

    /** Bounding instrument. */
    BoundingInstrument *m_boundingInstrument;

    /** Paint forwarding instrument. */
    ForwardingInstrument *m_paintForwardingInstrument;


    /* Animation-related stuff. */

    /** Viewport animator. */
    QnViewportAnimator *m_viewportAnimator;

    /** Curtain item. */
    QWeakPointer<QnCurtainItem> m_curtainItem;

    /** Curtain animator. */
    QnCurtainAnimator *m_curtainAnimator;


    /* Helpers. */

    /** Stored dummy scene. */
    QGraphicsScene *m_dummyScene;
};

#endif // QN_WORKBENCH_MANAGER_H
