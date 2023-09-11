// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_INSTRUMENT_MANAGER_P_H
#define QN_INSTRUMENT_MANAGER_P_H

#include <ui/animation/animation_timer.h>

#include "installation_mode.h"
#include "scene_event_filter.h" /* For SceneDestructionListener and SceneEventFilterItem. */

template<class T>
class InstrumentEventDispatcher;

class Instrument;
class InstrumentPaintSyncer;

class InstrumentManagerPrivate final: public QObject, protected SceneDestructionListener
{
public:
    InstrumentManagerPrivate();

private:
    virtual void destroyed(SceneEventFilterItem *item) override;

    void tick(int deltaTime);

    void init();

    void installInstrumentInternal(Instrument *instrument, InstallationMode::Mode mode, Instrument *reference);

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

    void at_viewportWatcher_destroyed(QObject *viewportWatcher);

    void addSyncedViewport(QWidget *viewport);

    QWidget *syncedViewport() const;

    void removeSyncedViewport(QWidget *viewport);

    void reinstallPaintSyncer(QWidget *oldViewport, QWidget *newViewport);

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

    InstrumentPaintSyncer *paintSyncer;
    qint64 totalTickTime;
    QList<QWidget *> syncedViewports;

    QSet<QGraphicsItem *> delayedItems;
    bool pendingDelayedItemProcessing;

    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();

private:
    Q_DECLARE_PUBLIC(InstrumentManager);
};

#endif // QN_INSTRUMENT_MANAGER_P_H
