#ifndef QN_INSTRUMENT_MANAGER_P_H
#define QN_INSTRUMENT_MANAGER_P_H

#include "sceneeventfilter.h" /* For SceneDestructionListener and SceneEventFilterItem. */
#include "installationmode.h"

template<class T>
class InstrumentEventDispatcher;

class Instrument;

class InstrumentManagerPrivate: protected SceneDestructionListener {
public:
    InstrumentManagerPrivate();

private:
    virtual void destroyed(SceneEventFilterItem *item) override;

    void init();

    void installInstrumentInternal(Instrument *instrument, InstallationMode::Mode mode);

    void uninstallInstrumentInternal(Instrument *instrument);

    void registerSceneInternal(QGraphicsScene *newScene);

    void unregisterSceneInternal();

    void registerViewInternal(QGraphicsView *view);

    void unregisterViewInternal(QObject *view, bool viewIsBeingDestroyed);

    void registerViewportInternal(QGraphicsView *view);

    void unregisterViewportInternal(QObject *viewport);

    void registerItemInternal(QGraphicsItem *item);

    void unregisterItemInternal(QGraphicsItem *item);

    void at_view_destroyed(QObject *view);

    void at_viewport_destroyed(QObject *viewport);

private:
    InstrumentManager *const q_ptr;
    QGraphicsScene *scene;
    bool sceneIsBeingDestroyed;
    SceneEventFilterItem *filterItem;
    QSet<QGraphicsView *> views;
    QSet<QGraphicsItem *> items;
    QList<Instrument *> instruments;
    
    InstrumentEventDispatcher<QGraphicsScene> *sceneDispatcher;
    InstrumentEventDispatcher<QGraphicsView> *viewDispatcher;
    InstrumentEventDispatcher<QWidget> *viewportDispatcher;
    InstrumentEventDispatcher<QGraphicsItem> *itemDispatcher;

private:
    Q_DECLARE_PUBLIC(InstrumentManager);
};

#endif // QN_INSTRUMENT_MANAGER_P_H