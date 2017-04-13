#include "instrument_manager.h"
#include "instrument_manager_p.h"
#include <cassert>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <utils/common/warnings.h>
#include <ui/animation/animation_event.h>
#include "instrument.h"
#include "instrument_event_dispatcher.h"
#include "instrument_paint_syncer.h"
#include "animation_instrument.h"

namespace {
    /** Name of the property to store a list of scene's instrument managers. */
    const char *managerPropertyName = "_qn_instrumentManager";

    const char *viewportWatcherName = "_qn_viewportWatcher";
}

Q_DECLARE_METATYPE(InstrumentManager *);

InstrumentManagerPrivate::InstrumentManagerPrivate():
    q_ptr(NULL),
    scene(NULL),
    sceneIsBeingDestroyed(false),
    filterItem(NULL),
    sceneDispatcher(NULL),
    viewDispatcher(NULL),
    viewportDispatcher(NULL),
    itemDispatcher(NULL),
    totalTickTime(0),
    pendingDelayedItemProcessing(false)
{}

void InstrumentManagerPrivate::init() {
    Q_Q(InstrumentManager);

    sceneDispatcher    = new InstrumentEventDispatcher<QGraphicsScene>(q);
    viewDispatcher     = new InstrumentEventDispatcher<QGraphicsView>(q);
    viewportDispatcher = new InstrumentEventDispatcher<QWidget>(q);
    itemDispatcher     = new InstrumentEventDispatcher<QGraphicsItem>(q);

    paintSyncer        = new InstrumentPaintSyncer(q);
    paintSyncer->addListener(this);
    startListening();
}

void InstrumentManagerPrivate::reinstallPaintSyncer(QWidget *oldViewport, QWidget *newViewport) {
    if(oldViewport == newViewport)
        return;

    if(oldViewport != NULL)
        oldViewport->removeEventFilter(paintSyncer);

    if(newViewport != NULL)
        newViewport->installEventFilter(paintSyncer);
}

void InstrumentManagerPrivate::addSyncedViewport(QWidget *viewport) {
    QWidget *oldSyncedViewport = syncedViewport();

    syncedViewports.push_back(viewport);

    reinstallPaintSyncer(oldSyncedViewport, syncedViewport());
}

QWidget *InstrumentManagerPrivate::syncedViewport() const {
    if(syncedViewports.empty()) {
        return NULL;
    } else {
        return syncedViewports.back();
    }
}

void InstrumentManagerPrivate::removeSyncedViewport(QWidget *viewport) {
    QWidget *oldSyncedViewport = syncedViewport();

    syncedViewports.removeAll(viewport);

    reinstallPaintSyncer(oldSyncedViewport, syncedViewport());
}

void InstrumentManagerPrivate::destroyed(SceneEventFilterItem *item) {
    NX_ASSERT(filterItem == item);

    /* Old filter item is already half-destroyed. */
    filterItem = NULL;

    /* Scene is being destroyed, unregister it. */
    sceneIsBeingDestroyed = true;
    unregisterSceneInternal();
    sceneIsBeingDestroyed = false;
}

void InstrumentManagerPrivate::tick(int deltaTime) {
    totalTickTime += deltaTime;

    QWidget *syncedViewport = this->syncedViewport();
    if(syncedViewport == NULL)
        return;

    AnimationEvent event(totalTickTime, deltaTime);
    viewportDispatcher->eventFilter(syncedViewport, &event);
}

void InstrumentManagerPrivate::installInstrumentInternal(Instrument *instrument, InstallationMode::Mode mode, Instrument *reference) {
    /* Initialize instrument. */
    instrument->setScene(scene);
    instrument->m_manager = q_func();

    /* Install instrument. */
    sceneDispatcher->installInstrument(instrument, mode, reference);
    viewDispatcher->installInstrument(instrument, mode, reference);
    viewportDispatcher->installInstrument(instrument, mode, reference);
    itemDispatcher->installInstrument(instrument, mode, reference);

    /* Notify. */
    instrument->sendInstalledNotifications(true);
}

void InstrumentManagerPrivate::uninstallInstrumentInternal(Instrument *instrument) {
    /* Don't let instrument use the scene if it's being destroyed. */
    if (sceneIsBeingDestroyed)
        instrument->setScene(nullptr);

    /* Notify. */
    instrument->sendInstalledNotifications(false);

    /* Uninstall instrument. */
    sceneDispatcher->uninstallInstrument(instrument);
    viewDispatcher->uninstallInstrument(instrument);
    viewportDispatcher->uninstallInstrument(instrument);
    itemDispatcher->uninstallInstrument(instrument);

    /* Deinitialize instrument. */
    instrument->m_scene = NULL;
    instrument->m_manager = NULL;
}

void InstrumentManagerPrivate::registerSceneInternal(QGraphicsScene *newScene) {
    NX_ASSERT(scene == NULL && newScene != NULL);

    /* Update scene and filter item. */
    scene = newScene;
    filterItem = new SceneEventFilterItem();
    filterItem->setDestructionListener(this);
    filterItem->setEventFilter(itemDispatcher);
    scene->addItem(filterItem);

    /* Install instruments. */
    foreach (Instrument *instrument, instruments)
        installInstrumentInternal(instrument, InstallationMode::InstallFirst, NULL);

    /* Store self as scene's instrument manager. */
    scene->setProperty(managerPropertyName, QVariant::fromValue<InstrumentManager *>(q_func()));

    /* Register scene with the scene event dispatcher. */
    sceneDispatcher->registerTarget(newScene);

    /* And only then install event filter. */
    scene->installEventFilter(sceneDispatcher);

    /* Notify about scene change. */
    emit q_func()->sceneChanged();
}

void InstrumentManagerPrivate::unregisterSceneInternal() {
    NX_ASSERT(scene != NULL);

    /* First remove event filter. */
    scene->removeEventFilter(sceneDispatcher);

    /* Unregister scene with the scene event dispatcher. */
    sceneDispatcher->unregisterTarget(scene);

    /* Remove self as scene's instrument manager. */
    scene->setProperty(managerPropertyName, QVariant());

    /* Unregister view and items. */
    while(!views.empty())
        unregisterViewInternal(*views.begin(), false);

    while(!items.empty())
        unregisterItemInternal(*items.begin());

    /* Uninstall instruments. */
    foreach (Instrument *instrument, instruments)
        uninstallInstrumentInternal(instrument);

    /* Clear scene and filter item. */
    scene = NULL;
    if (filterItem != NULL) { /* We may get called from filter item's destructor, hence the check for NULL. */
        filterItem->setDestructionListener(NULL);
        delete filterItem;
        filterItem = NULL;
    }

    /* Notify about scene change. */
    emit q_func()->sceneChanged();
}

void InstrumentManagerPrivate::registerViewInternal(QGraphicsView *view) {
    views.insert(view);
    view->installEventFilter(viewDispatcher);
    QObject::connect(view, &QObject::destroyed, q_func(), &InstrumentManager::at_view_destroyed);
    viewDispatcher->registerTarget(view);

    registerViewportInternal(view);

    /* This one is important. Mouse tracking is disabled by default, so
     * instruments won't be getting mouse move events if there are no items
     * on the scene. */
    view->viewport()->setMouseTracking(true);
}

void InstrumentManagerPrivate::unregisterViewInternal(QObject *view, bool viewIsBeingDestroyed) {
    if (!viewIsBeingDestroyed)
        unregisterViewportInternal(static_cast<QGraphicsView *>(view)->viewport());

    views.remove(static_cast<QGraphicsView *>(view));
    view->removeEventFilter(viewDispatcher);
    QObject::disconnect(view, NULL, q_func(), NULL);
    viewDispatcher->unregisterTarget(static_cast<QGraphicsView *>(view));
}

void InstrumentManagerPrivate::registerViewportInternal(QGraphicsView *view) {
    view->viewport()->installEventFilter(viewportDispatcher);

    QObject::connect(view->viewport(), &QObject::destroyed, q_func(), &InstrumentManager::at_viewport_destroyed);

    QWidget *viewport = view->viewport();
    addSyncedViewport(viewport);
    viewportDispatcher->registerTarget(viewport);

    /* Create a viewport destruction watcher.
     * It will be destroyed before the viewport's destructed gets a chance to send some events
     * that may get into our handlers and mess everything up. */
    QObject *viewportWatcher = new QObject(view->viewport());
    viewportWatcher->setObjectName(QLatin1String(viewportWatcherName));
    QObject::connect(viewportWatcher, &QObject::destroyed, q_func(), &InstrumentManager::at_viewportWatcher_destroyed);
}

void InstrumentManagerPrivate::unregisterViewportInternal(QObject *viewport) {
    QObject *viewportWatcher = viewport->findChild<QObject *>(QLatin1String(viewportWatcherName));
    if(viewportWatcher != NULL) {
        QObject::disconnect(viewportWatcher, NULL, q_func(), NULL);
        delete viewportWatcher;
    }

    viewport->removeEventFilter(viewportDispatcher);

    QObject::disconnect(viewport, NULL, q_func(), NULL);

    QWidget *viewportWidget = static_cast<QWidget *>(viewport);
    removeSyncedViewport(viewportWidget);
    viewportDispatcher->unregisterTarget(viewportWidget);
}

void InstrumentManagerPrivate::registerItemInternal(QGraphicsItem *item) {
    items.insert(item);

    item->installSceneEventFilter(filterItem);

    itemDispatcher->registerTarget(item);
}

void InstrumentManagerPrivate::unregisterItemInternal(QGraphicsItem *item) {
    items.remove(item);

    if(filterItem != NULL)
        item->removeSceneEventFilter(filterItem); /* We may get called from filter item's destructor, hence the check for NULL. */

    itemDispatcher->unregisterTarget(item);
}

void InstrumentManagerPrivate::at_view_destroyed(QObject *view) {
    /* Technically, we should never get here. */
    unregisterViewInternal(view, true);
}

void InstrumentManagerPrivate::at_viewport_destroyed(QObject *viewport) {
    /* We may get here in two cases:
     *
     * 1. View is being destroyed.
     * 2. Viewport is being changed. In this case view is alive and we just need to switch to new viewport. */
    QObject *view = viewport->parent();

    unregisterViewportInternal(viewport);

    QGraphicsView *localView = dynamic_cast<QGraphicsView *>(view);
    if (localView == NULL) {
        /* View is being destroyed. */
        unregisterViewInternal(view, true);
    } else {
        /* Viewport is being changed. */
        registerViewportInternal(localView);
    }
}

void InstrumentManagerPrivate::at_viewportWatcher_destroyed(QObject *viewportWatcher) {
    at_viewport_destroyed(viewportWatcher->parent());
}

InstrumentManager::InstrumentManager(QObject *parent):
    QObject(parent),
    d_ptr(new InstrumentManagerPrivate())
{
    const_cast<InstrumentManager *&>(d_ptr->q_ptr) = this;

    d_func()->init();

    installInstrument(new AnimationInstrument(this));
}

InstrumentManager::~InstrumentManager() {
    Q_D(InstrumentManager);

    if (d->scene != NULL)
        unregisterScene(d->scene);

    delete d_ptr;
}

QGraphicsScene *InstrumentManager::scene() const {
    return d_func()->scene;
}

const QSet<QGraphicsView *> &InstrumentManager::views() const {
    return d_func()->views;
}

const QSet<QGraphicsItem *> &InstrumentManager::items() const {
    return d_func()->items;
}

const QList<Instrument *> &InstrumentManager::instruments() const {
    return d_func()->instruments;
}

bool InstrumentManager::isAnimationEnabled() const
{
    return d_func()->timer()->isActive();
}

void InstrumentManager::setAnimationsEnabled(bool enabled)
{
    if (isAnimationEnabled() == enabled)
        return;

    if (enabled)
        d_func()->timer()->activate();
    else
        d_func()->timer()->deactivate();
}

bool InstrumentManager::installInstrument(Instrument *instrument, InstallationMode::Mode mode, Instrument *reference) {
    Q_D(InstrumentManager);

    if (instrument == NULL) {
        qnNullWarning(instrument);
        return false;
    }

    if (instrument->isInstalled()) {
        qnWarning("Instrument is already installed.");
        return false;
    }

    if (instrument->thread() != thread()) {
        qnWarning("Cannot install instrument that belongs to another thread.");
        return false;
    }

    if (mode < 0 && mode >= InstallationModeCount) {
        qnWarning("Unknown installation mode '%1'.", static_cast<int>(mode));
        return false;
    }

    /* Update local records. */
    insertInstrument(instrument, mode, reference, &d->instruments);

    if (d->scene == NULL)
        return true; /* We'll register it for real once we have the scene. */

    d->installInstrumentInternal(instrument, mode, reference);
    return true;
}

bool InstrumentManager::uninstallInstrument(Instrument *instrument) {
    Q_D(InstrumentManager);

    if (instrument == NULL) {
        qnNullWarning(instrument);
        return false;
    }

    if (!instrument->isInstalled())
        return false;

    if (instrument->manager() != this) {
        qnWarning("Cannot uninstall an instrument that is installed into another instrument manager.");
        return false;
    }

    /* Update local records. */
    d->instruments.removeOne(instrument);

    if (d->scene == NULL)
        return true; /* It's not installed for real. */

    d->uninstallInstrumentInternal(instrument);
    return true;
}

void InstrumentManager::registerScene(QGraphicsScene *scene) {
    Q_D(InstrumentManager);

    if (scene == NULL) {
        qnNullWarning(scene);
        return;
    }

    if (d->scene != NULL) {
        qnWarning("Cannot register several graphics scenes.");
        return;
    }

    if(instance(scene) != NULL) {
        qnWarning("Given scene is already assigned an instrument manager.");
        return;
    }

    d->registerSceneInternal(scene);
}

void InstrumentManager::unregisterScene(QGraphicsScene *scene) {
    Q_D(InstrumentManager);

    if (scene == NULL) {
        qnNullWarning(scene);
        return;
    }

    if (d->scene != scene)
        return;

    d->unregisterSceneInternal();
}

void InstrumentManager::registerView(QGraphicsView *view) {
    Q_D(InstrumentManager);

    if (view == NULL) {
        qnNullWarning(view);
        return;
    }

    if (view->scene() == NULL) {
        qnWarning("Given view is not assigned to any scene.");
        return;
    }

    if (d->scene == NULL) {
        qnWarning("No scene is registered with this instrument manager, cannot register view.");
        return;
    }

    if (view->scene() != d->scene) {
        qnWarning("Given view is assigned to a scene that differs from instrument manager's scene.");
        return;
    }

    if (d->views.contains(view))
        return; /* Re-registration is allowed. */

    d->registerViewInternal(view);
}

void InstrumentManager::unregisterView(QGraphicsView *view) {
    Q_D(InstrumentManager);

    if (view == NULL) {
        qnNullWarning(view);
        return;
    }

    if (!d->views.contains(view))
        return;

    d->unregisterViewInternal(view, false);
}

void InstrumentManager::registerItem(QGraphicsItem *item, bool delayed) {
    Q_D(InstrumentManager);

    if (item == NULL) {
        qnNullWarning(item);
        return;
    }

    if (item->scene() == NULL) {
        qnWarning("Given item does not belong to any scene.");
        return;
    }

    if (d->scene == NULL) {
        qnWarning("No scene is registered with this instrument manager, cannot register item.");
        return;
    }

    if (item->scene() != d->scene) {
        qnWarning("Given item belongs to a scene that differs from instrument manager's scene.");
        return;
    }

    if (d->items.contains(item))
        return; /* Re-registration is allowed. */

    if(delayed) {
        d->delayedItems.insert(item);

        if(!d->pendingDelayedItemProcessing) {
            d->pendingDelayedItemProcessing = true;
            QMetaObject::invokeMethod(this, "at_delayedItemRegistrationRequested", Qt::QueuedConnection);
        }
    } else {
        d->delayedItems.remove(item);
        d->registerItemInternal(item);
    }
}

void InstrumentManager::unregisterItem(QGraphicsItem *item) {
    Q_D(InstrumentManager);

    if (item == NULL) {
        qnNullWarning(item);
        return;
    }

    if (d->delayedItems.remove(item))
        return; /* Was scheduled to be registered, but didn't have a chance to. */

    if (!d->items.contains(item))
        return;

    d->unregisterItemInternal(item);
}

void InstrumentManager::at_view_destroyed(QObject *view) {
    d_func()->at_view_destroyed(view);
}

void InstrumentManager::at_viewport_destroyed(QObject *viewport) {
    d_func()->at_viewport_destroyed(viewport);
}

void InstrumentManager::at_viewportWatcher_destroyed(QObject *viewportWatcher) {
    d_func()->at_viewportWatcher_destroyed(viewportWatcher);
}

void InstrumentManager::at_delayedItemRegistrationRequested() {
    Q_D(InstrumentManager);

    while(!d->delayedItems.isEmpty())
        registerItem(*d->delayedItems.begin(), false);

    d->pendingDelayedItemProcessing = false;
}

InstrumentManager *InstrumentManager::instance(QGraphicsScene *scene) {
    if(!scene)
        return NULL;

    return scene->property(managerPropertyName).value<InstrumentManager *>();
}

AnimationTimer *InstrumentManager::animationTimer() const {
    AnimationInstrument *animationInstrument = instrument<AnimationInstrument>();
    if(animationInstrument != NULL) {
        return animationInstrument->animationTimer();
    } else {
        return NULL;
    }
}

AnimationTimer *InstrumentManager::animationTimer(QGraphicsScene *scene) {
    if(!scene)
        return NULL;

    if(InstrumentManager *manager = instance(scene))
        return manager->animationTimer();

    return NULL;
}

void InstallationMode::insertInstrument(Instrument *instrument, InstallationMode::Mode mode, Instrument *reference, QList<Instrument *> *target) {
    NX_ASSERT(instrument != NULL && target != NULL);

    /* Note that event processing goes from the last element in the list to the first.
     * This is why we do push_front on InstallLast. Don't be surprised. */

    int index;
    switch(mode) {
    case InstrumentManager::InstallLast:
        target->push_front(instrument);
        return;
    case InstrumentManager::InstallFirst:
        target->push_back(instrument);
        return;
    case InstrumentManager::InstallBefore:
        index = target->indexOf(reference);
        if(index == -1) {
            insertInstrument(instrument, InstallationMode::InstallFirst, reference, target);
        } else {
            target->insert(index + 1, instrument);
        }
        return;
    case InstrumentManager::InstallAfter:
        index = target->indexOf(reference);
        if(index == -1) {
            insertInstrument(instrument, InstallationMode::InstallLast, reference, target);
        } else {
            target->insert(index, instrument);
        }
        return;
    default:
        qnWarning("Unknown instrument installation mode '%1', using InstallFirst instead.", static_cast<int>(mode));
        insertInstrument(instrument, InstrumentManager::InstallFirst, reference, target);
        return;
    }
}
