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
class QnDisplayEntity;
class QnDisplayWidget;
class QnViewportAnimator;
class QnWidgetAnimator;

class QnDisplaySynchronizer: public QObject, protected AnimationTimerListener, protected QnSceneUtility {
    Q_OBJECT;
public:
    /**
     * Display synchronizer assigns items to layers.
     */
    enum Layer {
        BACK_LAYER,
        PINNED_LAYER,
        UNPINNED_LAYER,
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

    QRectF entityGeometry(QnDisplayEntity *entity) const;

    QRectF zoomedEntityGeometry() const;

    QRectF boundingGeometry() const;

    QRectF viewportGeometry() const;

    void synchronize(QnDisplayEntity *entity);

    void fitInView();

    void bringToFront(const QList<QGraphicsItem *> &items);

    void bringToFront(QGraphicsItem *item);

    void bringToFront(QnDisplayEntity *entity);

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
    
    QnWidgetAnimator *newWidgetAnimator(QnDisplayWidget *widget);
    
    void updateWidgetGeometry(QnDisplayEntity *entity, bool animate);

protected slots:
    void updateSceneBounds();

    void at_model_entityAdded(QnDisplayEntity *entity);
    void at_model_entityAboutToBeRemoved(QnDisplayEntity *entity);
    
    void at_state_modeChanged();
    void at_state_selectedEntityChanged(QnDisplayEntity *oldSelectedEntity, QnDisplayEntity *newSelectedEntity);
    void at_state_zoomedEntityChanged(QnDisplayEntity *oldZoomedEntity, QnDisplayEntity *newZoomedEntity);

    void at_entity_geometryUpdated(const QRect &oldGeometry, const QRect &newGeometry);
    void at_entity_geometryDeltaUpdated();
    void at_entity_rotationUpdated();

    void at_viewport_transformationChanged();

private:
    struct EntityData {
        EntityData(): widget(NULL), animator(NULL) {}
        EntityData(QnDisplayWidget *widget, QnWidgetAnimator *animator): widget(widget), animator(animator) {}

        QnDisplayWidget *widget;
        QnWidgetAnimator *animator;
    };

    QnDisplayState *m_state;
    InstrumentManager *m_manager;
    QGraphicsScene *m_scene;
    QGraphicsView *m_view;
    QHash<QnDisplayEntity *, EntityData> m_dataByEntity;
    QnViewportAnimator *m_viewportAnimator;
    AnimationTimer *m_updateTimer;
    BoundingInstrument *m_boundingInstrument;
    TransformListenerInstrument *m_transformListenerInstrument;

    qreal m_frontZ;
};

#endif // QN_DISPLAY_SYNCHRONIZER_H
