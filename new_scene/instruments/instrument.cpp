#include "instrument.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <utility/Warnings.h>
#include <utility/CheckedCast.h>
#include "instrumentmanager.h"


Instrument::Instrument(const QSet<QEvent::Type> &sceneEventTypes, const QSet<QEvent::Type> &viewEventTypes, const QSet<QEvent::Type> &viewportEventTypes, bool watchesItemEvents, QObject *parent):
    QObject(parent),
    m_manager(NULL),
    m_scene(NULL),
    m_watchesItemEvents(watchesItemEvents)
{
    m_watchedEventTypes[SCENE]    = sceneEventTypes;
    m_watchedEventTypes[VIEW]     = viewEventTypes;
    m_watchedEventTypes[VIEWPORT] = viewportEventTypes;
}

Instrument::~Instrument() {
    ensureUninstalled();
}

bool Instrument::watches(WatchedType type) const {
    if (type < 0 || type > WATCHED_TYPE_COUNT) {
        qnWarning("Invalid watched type %1", type);
        return false;
    }

    if (type == ITEM) {
        return m_watchesItemEvents;
    } else {
        return !m_watchedEventTypes[type].empty();
    }
}

void Instrument::ensureUninstalled() {
    if (isInstalled())
        m_manager->uninstallInstrument(this);
}

QGraphicsView *Instrument::view(QWidget *viewport) const {
    return checked_cast<QGraphicsView *>(viewport->parent());
}

QList<QGraphicsItem *> Instrument::items(QGraphicsView *view, const QPoint &viewPos) const {
    if (m_scene == NULL) {
        if (!isInstalled()) {
            qnWarning("Instrument is not installed.");
        } else {
            qnWarning("Scene is being destroyed.");
        }

        return QList<QGraphicsItem *>();
    }

    return m_scene->items(view->mapToScene(QRect(viewPos, QSize(1, 1))), Qt::IntersectsItemShape, Qt::DescendingOrder, view == NULL ? QTransform() : view->viewportTransform());
}


QSet<QGraphicsView *> Instrument::views() const {
    if(isInstalled()) {
        return m_manager->views();
    } else {
        return QSet<QGraphicsView *>();
    }
}