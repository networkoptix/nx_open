#ifndef QN_DISPLAY_SYNCHRONIZER_H
#define QN_DISPLAY_SYNCHRONIZER_H

#include <QObject>
#include <ui/instruments/animationtimer.h>
#include <utils/common/scene_utility.h>
#include <utils/common/rect_set.h>

class QGraphicsScene;
class QGraphicsView;

class InstrumentManager;
class BoundingInstrument;
class TransformListenerInstrument;

class QnDisplayState;
class QnUiLayoutItem;
class QnDisplayWidget;
class QnViewportAnimator;
class QnWidgetAnimator;

class QnDisplaySynchronizer: public QObject, protected AnimationTimerListener, protected QnSceneUtility {
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
        PRE_ZOOMED_LAYER,
        ZOOMED_LAYER,
        FRONT_LAYER
    };

    QnDisplaySynchronizer(QnDisplayState *state, QGraphicsScene *scene, QGraphicsView *view, QObject *parent = NULL);

    virtual ~QnDisplaySynchronizer();

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

    QRectF entityGeometry(QnUiLayoutItem *entity) const;

    QRectF zoomedEntityGeometry() const;

    QRectF boundingGeometry() const;

    QRectF viewportGeometry() const;

    void fitInView();

    void bringToFront(const QList<QGraphicsItem *> &items);

    void bringToFront(QGraphicsItem *item);

    void bringToFront(QnUiLayoutItem *entity);

    Layer layer(QGraphicsItem *item);

    void setLayer(QGraphicsItem *item, Layer layer);

    void synchronize(QnUiLayoutItem *entity, bool animate = true);
    
    void synchronize(QnDisplayWidget *widget, bool animate = true);


public slots:
    void disableViewportChanges();
    void enableViewportChanges();

signals:
    void viewportGrabbed();
    void viewportUngrabbed();
    
    // TODO: remove
    void widgetAdded(QnDisplayWidget *widget);

protected:
    virtual void tick(int currentTime) override;
    
    QnWidgetAnimator *animator(QnDisplayWidget *widget);

    void synchronizeGeometry(QnUiLayoutItem *entity, bool animate);
    void synchronizeGeometry(QnDisplayWidget *widget, bool animate);
    void synchronizeLayer(QnUiLayoutItem *entity);
    void synchronizeLayer(QnDisplayWidget *widget);

    qreal layerFrontZ(Layer layer) const;
    Layer entityLayer(QnUiLayoutItem *entity) const;

protected slots:
    void synchronizeSceneBounds();

    void at_model_entityAdded(QnUiLayoutItem *entity);
    void at_model_entityAboutToBeRemoved(QnUiLayoutItem *entity);
    
    void at_state_modeChanged();
    void at_state_selectedEntityChanged(QnUiLayoutItem *oldSelectedEntity, QnUiLayoutItem *newSelectedEntity);
    void at_state_zoomedEntityChanged(QnUiLayoutItem *oldZoomedEntity, QnUiLayoutItem *newZoomedEntity);

    void at_entity_geometryChanged();
    void at_entity_geometryDeltaChanged();
    void at_entity_rotationChanged();
    void at_entity_flagsChanged();

    void at_viewport_transformationChanged();

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
    QHash<QnUiLayoutItem *, QnDisplayWidget *> m_widgetByEntity;

    /** Item to item properties mapping. */
    QHash<QGraphicsItem *, ItemProperties> m_propertiesByItem;

    /** Current front z displacement value. */
    qreal m_frontZ;
};

#endif // QN_DISPLAY_SYNCHRONIZER_H
