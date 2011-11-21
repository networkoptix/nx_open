#include "instrumentmanager.h"
#include "instrumentmanager_p.h"
#include <cassert>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <utils/common/warnings.h>
#include "instrument.h"
#include "instrumenteventdispatcher.h"

namespace {
    /** Name of the property to store a list of scene's instrument managers. */
    const char *managersPropertyName = "_qn_instrumentManagers";
}

/* For QVariant to work with it. */
Q_DECLARE_METATYPE(QList<InstrumentManager *>);

InstrumentManagerPrivate::InstrumentManagerPrivate():
    q_ptr(NULL),
    scene(NULL),
    sceneIsBeingDestroyed(false),
    filterItem(NULL),
    sceneDispatcher(NULL),
    viewDispatcher(NULL),
    viewportDispatcher(NULL),
    itemDispatcher(NULL)
{}

void InstrumentManagerPrivate::init() {
    Q_Q(InstrumentManager);

    sceneDispatcher    = new InstrumentEventDispatcher<QGraphicsScene>(q);
    viewDispatcher     = new InstrumentEventDispatcher<QGraphicsView>(q);
    viewportDispatcher = new InstrumentEventDispatcher<QWidget>(q);
    itemDispatcher     = new InstrumentEventDispatcher<QGraphicsItem>(q);
}

void InstrumentManagerPrivate::destroyed(SceneEventFilterItem *item) {
    assert(filterItem == item);

    /* Old filter item is already half-destroyed. */
    filterItem = NULL;

    /* Scene is being destroyed, unregister it. */
    sceneIsBeingDestroyed = true;
    unregisterSceneInternal();
    sceneIsBeingDestroyed = false;
}

void InstrumentManagerPrivate::installInstrumentInternal(Instrument *instrument) {
    /* Initialize instrument. */
    instrument->m_scene = scene;
    instrument->m_manager = q_func();

    /* Install instrument. */
    sceneDispatcher->installInstrument(instrument);
    viewDispatcher->installInstrument(instrument);
    viewportDispatcher->installInstrument(instrument);
    itemDispatcher->installInstrument(instrument);

    /* Notify. */
    instrument->sendInstalledNotifications(true);
}

void InstrumentManagerPrivate::uninstallInstrumentInternal(Instrument *instrument) {
    /* Don't let instrument use the scene if it's being destroyed. */
    if (sceneIsBeingDestroyed)
        instrument->m_scene = NULL;

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
    assert(scene == NULL && newScene != NULL);

    Q_Q(InstrumentManager);

    /* Update scene and filter item. */
    scene = newScene;
    filterItem = new SceneEventFilterItem();
    filterItem->setDestructionListener(this);
    filterItem->setEventFilter(itemDispatcher);
    scene->addItem(filterItem);

    /* Install instruments. */
    foreach (Instrument *instrument, instruments)
        installInstrumentInternal(instrument);

    /* Store self in scene's list of instrument managers. */
    QList<InstrumentManager *> managers = q->managersOf(scene);
    managers.push_back(q);
    scene->setProperty(managersPropertyName, QVariant::fromValue(managers));

    /* Register scene with the scene event dispatcher. */
    sceneDispatcher->registerTarget(newScene);

    /* And only then install event filter. */
    scene->installEventFilter(sceneDispatcher);
}

void InstrumentManagerPrivate::unregisterSceneInternal() {
    assert(scene != NULL);

    Q_Q(InstrumentManager);

    /* First remove event filter. */
    scene->removeEventFilter(sceneDispatcher);

    /* Unregister scene with the scene event dispatcher. */
    sceneDispatcher->unregisterTarget(scene);

    /* Remove self from scene's list of instrument managers. */
    QList<InstrumentManager *> managers = q->managersOf(scene);
    managers.removeOne(q);
    scene->setProperty(managersPropertyName, QVariant::fromValue(managers));

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
}

void InstrumentManagerPrivate::registerViewInternal(QGraphicsView *view) {
    views.insert(view);
    view->installEventFilter(viewDispatcher);
    QObject::connect(view, SIGNAL(destroyed(QObject *)), q_func(), SLOT(at_view_destroyed(QObject *)));
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
    
    QObject::connect(view->viewport(), SIGNAL(destroyed(QObject *)), q_func(), SLOT(at_viewport_destroyed(QObject *)));

    viewportDispatcher->registerTarget(view->viewport());
}

void InstrumentManagerPrivate::unregisterViewportInternal(QObject *viewport) {
    viewport->removeEventFilter(viewportDispatcher);

    QObject::disconnect(viewport, NULL, q_func(), NULL);

    viewportDispatcher->unregisterTarget(static_cast<QWidget *>(viewport));
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

InstrumentManager::InstrumentManager(QObject *parent):
    QObject(parent),
    d_ptr(new InstrumentManagerPrivate())
{
    const_cast<InstrumentManager *&>(d_ptr->q_ptr) = this;

    d_func()->init();
}

InstrumentManager::~InstrumentManager() {
    Q_D(InstrumentManager);

    if (d->scene != NULL)
        unregisterScene(d->scene);
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

bool InstrumentManager::installInstrument(Instrument *instrument) {
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

    /* Update local records. */
    d->instruments.push_back(instrument);

    if (d->scene == NULL)
        return true; /* We'll register it for real once we have the scene. */

    d->installInstrumentInternal(instrument);
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

void InstrumentManager::registerItem(QGraphicsItem *item) {
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

    d->registerItemInternal(item);
}

void InstrumentManager::unregisterItem(QGraphicsItem *item) {
    Q_D(InstrumentManager);

    if (item == NULL) {
        qnNullWarning(item);
        return;
    }

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

QList<InstrumentManager *> InstrumentManager::managersOf(QGraphicsScene *scene) {
    return scene->property(managersPropertyName).value<QList<InstrumentManager *> >();
}