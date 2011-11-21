#ifndef QN_WORKBENCH_MANAGER_H
#define QN_WORKBENCH_MANAGER_H

#include <QObject>
#include <ui/animation/animation_timer.h>
#include <utils/common/scene_utility.h>
#include <utils/common/rect_set.h>

class QGraphicsScene;
class QGraphicsView;

class InstrumentManager;
class BoundingInstrument;
class TransformListenerInstrument;
class ActivityListenerInstrument;

class QnWorkbench;
class QnWorkbenchItem;
class QnWorkbenchLayout;
class QnDisplayWidget;
class QnViewportAnimator;
class QnWidgetAnimator;
class QnCurtainAnimator;
class QnCurtainItem;

/**
 * This class ties a workbench, a scene and a view together.
 * 
 * It presents some low-level functions for viewport and item manipulation.
 */
class QnWorkbenchManager: public QObject, protected AnimationTimerListener, protected QnSceneUtility {
    Q_OBJECT;
public:
    /**
     * Layer of an item.
     */
    enum Layer {
        BACK_LAYER,
        PINNED_LAYER,
        PINNED_SELECTED_LAYER,
        UNPINNED_LAYER,
        UNPINNED_SELECTED_LAYER,
        CURTAIN_LAYER,
        ZOOMED_LAYER,
        FRONT_LAYER
    };

    /**
     * Constructor.
     * 
     * \param parent                    Parent object for this workbench manager.
     */
    QnWorkbenchManager(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchManager();

    /**
     * \returns                         Instrument manager owned by this workbench manager. 
     */
    InstrumentManager *manager() const {
        return m_manager;
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
     * Note that this function never returns NULL.
     * 
     * \returns                         Current workbench of this workbench manager. 
     */
    QnWorkbench *workbench() const {
        return m_workbench;
    }

    /**
     * Note that workbench manager does not take ownership of the supplied workbench.
     *
     * \param workbench                 New workbench for this workbench manager.
     *                                  If NULL is supplied, an empty workbench
     *                                  owned by this workbench manager is used.
     */
    void setWorkbench(QnWorkbench *workbench);

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

    QGraphicsView *view() const {
        return m_view;
    }

    void setView(QGraphicsView *view);

    /**
     * \param item                      Item to get widget for.
     */
    QnDisplayWidget *widget(QnWorkbenchItem *item) const;

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


    void bringToFront(const QList<QGraphicsItem *> &items);

    void bringToFront(QGraphicsItem *item);

    void bringToFront(QnWorkbenchItem *item);


    Layer layer(QGraphicsItem *item) const;

    void setLayer(QGraphicsItem *item, Layer layer);


    void synchronize(QnWorkbenchItem *item, bool animate = true);
    
    void synchronize(QnDisplayWidget *widget, bool animate = true);


public slots:
    void disableViewportChanges();
    void enableViewportChanges();

signals:
    void viewportGrabbed();
    void viewportUngrabbed();

protected:
    virtual void tick(int currentTime) override;
    
    QnWidgetAnimator *animator(QnDisplayWidget *widget);

    void synchronizeGeometry(QnWorkbenchItem *item, bool animate);
    void synchronizeGeometry(QnDisplayWidget *widget, bool animate);
    void synchronizeLayer(QnWorkbenchItem *item);
    void synchronizeLayer(QnDisplayWidget *widget);
    void synchronizeSceneBounds();

    qreal layerFront(Layer layer) const;
    Layer synchronizedLayer(QnWorkbenchItem *item) const;

    void addItemInternal(QnWorkbenchItem *item);
    void removeItemInternal(QnWorkbenchItem *item);

    void changeZoomedItem(QnWorkbenchItem *item);
    void changeSelectedItem(QnWorkbenchItem *item);

protected slots:
    void at_viewport_animationFinished();

    void at_state_itemAdded(QnWorkbenchItem *item);
    void at_state_itemAboutToBeRemoved(QnWorkbenchItem *item);
    
    void at_state_aboutToBeDestroyed();
    void at_state_modeChanged();
    void at_state_selectedItemChanged();
    void at_state_zoomedItemChanged();
    void at_state_layoutChanged();

    void at_item_geometryChanged();
    void at_item_geometryDeltaChanged();
    void at_item_rotationChanged();
    void at_item_flagsChanged();

    void at_viewport_transformationChanged();

    void at_activityStopped();
    void at_activityStarted();

    void at_curtained();
    void at_uncurtained();

    void at_widget_destroyed();
    void at_scene_destroyed();
    void at_view_destroyed();

private:
    struct ItemProperties {
        ItemProperties(): animator(NULL), layer(BACK_LAYER) {}

        QnWidgetAnimator *animator;
        Layer layer;
    };

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
    QHash<QnWorkbenchItem *, QnDisplayWidget *> m_widgetByItem;

    /** Item to item properties mapping. */
    QHash<QGraphicsItem *, ItemProperties> m_propertiesByItem;

    /** Current front z displacement value. */
    qreal m_frontZ;

    /** Currently selected item. */
    QnWorkbenchItem *m_selectedItem;

    /** Currently zoomed item. */
    QnWorkbenchItem *m_zoomedItem;


    /* Instruments. */

    /** Instrument manager owned by this workbench manager. */
    InstrumentManager *m_manager;

    /** Transformation listener instrument. */
    TransformListenerInstrument *m_transformListenerInstrument;

    /** Activity listener instrument. */
    ActivityListenerInstrument *m_activityListenerInstrument;

    /** Bounding instrument. */
    BoundingInstrument *m_boundingInstrument;


    /* Animation-related stuff. */

    /** Timer that is used to update the viewport. */
    AnimationTimer *m_updateTimer;

    /** Viewport animator. */
    QnViewportAnimator *m_viewportAnimator;

    /** Curtain item. */
    QWeakPointer<QnCurtainItem> m_curtainItem;

    /** Curtain animator. */
    QnCurtainAnimator *m_curtainAnimator;


    /* Helpers. */

    /** Stored dummy scene. */
    QGraphicsScene *m_dummyScene;

    /** Stored dummy workbench. */
    QnWorkbench *m_dummyWorkbench;
};

#endif // QN_WORKBENCH_MANAGER_H
