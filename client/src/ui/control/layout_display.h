#ifndef QN_DISPLAY_SYNCHRONIZER_H
#define QN_DISPLAY_SYNCHRONIZER_H

#include <QObject>
#include <ui/graphics/instruments/animationtimer.h>
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

    QnLayoutDisplay(QnDisplayState *state, QGraphicsScene *scene, QGraphicsView *view, QObject *parent = NULL);

    virtual ~QnLayoutDisplay();

    QnDisplayState *state() const {
        return m_state;
    }

    InstrumentManager *manager() const {
        return m_manager;
    }

    QGraphicsScene *scene() const {
        return m_scene;
    }

    QGraphicsView *view() const {
        return m_view;
    }

    QRectF entityGeometry(QnLayoutItemModel *entity) const;

    QRectF zoomedEntityGeometry() const;

    QRectF boundingGeometry() const;

    QRectF viewportGeometry() const;

    void fitInView();

    void bringToFront(const QList<QGraphicsItem *> &items);

    void bringToFront(QGraphicsItem *item);

    void bringToFront(QnLayoutItemModel *entity);

    Layer layer(QGraphicsItem *item);

    void setLayer(QGraphicsItem *item, Layer layer);

    void synchronize(QnLayoutItemModel *entity, bool animate = true);
    
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

    void synchronizeGeometry(QnLayoutItemModel *entity, bool animate);
    void synchronizeGeometry(QnDisplayWidget *widget, bool animate);
    void synchronizeLayer(QnLayoutItemModel *entity);
    void synchronizeLayer(QnDisplayWidget *widget);

    qreal layerFrontZ(Layer layer) const;
    Layer entityLayer(QnLayoutItemModel *entity) const;

protected slots:
    void synchronizeSceneBounds();

    void at_model_itemAdded(QnLayoutItemModel *entity);
    void at_model_itemAboutToBeRemoved(QnLayoutItemModel *entity);
    
    void at_state_modeChanged();
    void at_state_selectedEntityChanged(QnLayoutItemModel *oldSelectedEntity, QnLayoutItemModel *newSelectedEntity);
    void at_state_zoomedEntityChanged(QnLayoutItemModel *oldZoomedEntity, QnLayoutItemModel *newZoomedEntity);

    void at_item_geometryChanged();
    void at_item_geometryDeltaChanged();
    void at_item_rotationChanged();
    void at_item_flagsChanged();

    void at_viewport_transformationChanged();

    void at_activityStopped();
    void at_activityStarted();

    void at_curtained();
    void at_uncurtained();

private:
    struct ItemProperties {
        ItemProperties(): animator(NULL), layer(BACK_LAYER) {}

        QnWidgetAnimator *animator;
        Layer layer;
    };

private:
    QnDisplayState *m_state;
    InstrumentManager *m_manager;
    QGraphicsScene *m_scene;
    QGraphicsView *m_view;
    
    QnViewportAnimator *m_viewportAnimator;
    AnimationTimer *m_updateTimer;
    BoundingInstrument *m_boundingInstrument;
    TransformListenerInstrument *m_transformListenerInstrument;

    /** Entity to widget mapping. */
    QHash<QnLayoutItemModel *, QnDisplayWidget *> m_widgetByEntity;

    /** Item to item properties mapping. */
    QHash<QGraphicsItem *, ItemProperties> m_propertiesByItem;

    /** Current front z displacement value. */
    qreal m_frontZ;

    /** Activity listener instrument. */
    ActivityListenerInstrument *m_activityListenerInstrument;

    /** Curtain item. */
    QnCurtainItem *m_curtainItem;

    /** Curtain animator. */
    QnCurtainAnimator *m_curtainAnimator;
};

#endif // QN_DISPLAY_SYNCHRONIZER_H
