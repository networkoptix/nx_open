#ifndef QN_DISPLAY_SYNCHRONIZER_H
#define QN_DISPLAY_SYNCHRONIZER_H

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

class QnDisplayState;
class QnLayoutItemModel;
class QnDisplayWidget;
class QnViewportAnimator;
class QnWidgetAnimator;
class QnCurtainAnimator;
class QnCurtainItem;
class QnLayoutModel;

/**
 * This class ties a display state, a scene and a view together.
 * 
 * It presents some low-level functions for viewport and item manipulation.
 */
class QnLayoutDisplay: public QObject, protected AnimationTimerListener, protected QnSceneUtility {
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

    QnLayoutDisplay(QObject *parent = NULL);

    virtual ~QnLayoutDisplay();

    InstrumentManager *manager() const {
        return m_manager;
    }

    QnDisplayState *state() const {
        return m_state;
    }

    void setState(QnDisplayState *state);

    QGraphicsScene *scene() const {
        return m_scene;
    }

    void setScene(QGraphicsScene *scene);

    QGraphicsView *view() const {
        return m_view;
    }

    void setView(QGraphicsView *view);

    /**
     * \param item                      Item to get widget for.
     */
    QnDisplayWidget *widget(QnLayoutItemModel *item) const;

    /**
     * \param item                      Item to get enclosing geometry for.
     * \returns                         Given item's enclosing geometry in scene 
     *                                  coordinates as defined by the model.
     *                                  Note that actual geometry may differ because of
     *                                  aspect ration constraints.
     */
    QRectF itemEnclosingGeometry(QnLayoutItemModel *item) const;

    /**
     * \param item                      Item to get geometry for.
     * \param[out] enclosingGeometry    Item's enclosing geometry.
     * \returns                         Item's geometry in scene coordinates,
     *                                  taking aspect ratio constraints into account.
     *                                  Note that actual geometry of the item's widget
     *                                  may differ because of manual dragging / resizing / etc...
     */
    QRectF itemGeometry(QnLayoutItemModel *item, QRectF *enclosingGeometry = NULL) const;

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

    void bringToFront(QnLayoutItemModel *item);


    Layer layer(QGraphicsItem *item) const;

    void setLayer(QGraphicsItem *item, Layer layer);


    void synchronize(QnLayoutItemModel *item, bool animate = true);
    
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

    void synchronizeGeometry(QnLayoutItemModel *item, bool animate);
    void synchronizeGeometry(QnDisplayWidget *widget, bool animate);
    void synchronizeLayer(QnLayoutItemModel *item);
    void synchronizeLayer(QnDisplayWidget *widget);
    void synchronizeSceneBounds();

    qreal layerFront(Layer layer) const;
    Layer synchronizedLayer(QnLayoutItemModel *item) const;

    void addItemInternal(QnLayoutItemModel *item);
    void removeItemInternal(QnLayoutItemModel *item);

protected slots:
    void at_viewport_animationFinished();

    void at_layout_itemAdded(QnLayoutItemModel *item);
    void at_layout_itemAboutToBeRemoved(QnLayoutItemModel *item);
    
    void at_state_aboutToBeDestroyed();
    void at_state_modeChanged();
    void at_state_selectedItemChanged(QnLayoutItemModel *oldSelectedItem, QnLayoutItemModel *newSelectedItem);
    void at_state_zoomedItemChanged(QnLayoutItemModel *oldZoomedItem, QnLayoutItemModel *newZoomedItem);
    void at_state_layoutChanged(QnLayoutModel *oldLayout, QnLayoutModel *newLayout);

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
    QnDisplayState *m_state;
    QnLayoutModel *m_layout;
    QGraphicsScene *m_scene;
    QGraphicsView *m_view;

    /** Instrument manager owned by this display. */
    InstrumentManager *m_manager;

    /** Entity to widget mapping. */
    QHash<QnLayoutItemModel *, QnDisplayWidget *> m_widgetByItem;

    /** Item to item properties mapping. */
    QHash<QGraphicsItem *, ItemProperties> m_propertiesByItem;

    /** Current front z displacement value. */
    qreal m_frontZ;

    /** Timer that is used to update the viewport. */
    AnimationTimer *m_updateTimer;

    /** Transformation listener instrument. */
    TransformListenerInstrument *m_transformListenerInstrument;

    /** Activity listener instrument. */
    ActivityListenerInstrument *m_activityListenerInstrument;

    /** Bounding instrument. */
    BoundingInstrument *m_boundingInstrument;

    /** Viewport animator. */
    QnViewportAnimator *m_viewportAnimator;

    /** Curtain item. */
    QWeakPointer<QnCurtainItem> m_curtainItem;

    /** Curtain animator. */
    QnCurtainAnimator *m_curtainAnimator;

    /** Stored dummy scene. */
    QGraphicsScene *m_dummyScene;
};

#endif // QN_DISPLAY_SYNCHRONIZER_H
