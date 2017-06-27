#include "instrument.h"
#include <cassert>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QTabletEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QMoveEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QCloseEvent>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QShowEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QActionEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneDragDropEvent>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsSceneWheelEvent>
#include <QtWidgets/QGraphicsSceneHelpEvent>
#include <QtWidgets/QGraphicsSceneHoverEvent>
#include <ui/animation/animation_event.h>
#include <ui/animation/animation_timer.h>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#include "instrument_manager.h"

Instrument::Instrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent):
    base_type(parent),
    m_animationTimer(NULL)
{
    initialize();

    m_watchedEventTypes[Viewport] = viewportEventTypes;
    m_watchedEventTypes[View]     = viewEventTypes;
    m_watchedEventTypes[Scene]    = sceneEventTypes;
    m_watchedEventTypes[Item]     = itemEventTypes;
}

Instrument::Instrument(WatchedType type, const EventTypeSet &eventTypes, QObject *parent):
    base_type(parent)
{
    initialize();

    if(type < 0 || type >= WatchedTypeCount) {
        qnWarning("Invalid watched type %1", type);

        return;
    }

    m_watchedEventTypes[type] = eventTypes;
}

QGraphicsScene* Instrument::scene() const
{
    return m_scene;
}

void Instrument::setScene(QGraphicsScene* scene)
{
    if (m_scene == scene)
        return;

    m_scene = scene;
    emit sceneChanged();
}

void Instrument::initialize() {
    m_manager = NULL;
    m_scene = NULL;
    m_disabledCounter = 0;
}

Instrument::~Instrument() {
    ensureUninstalled();

    setItemConditions(QList<InstrumentItemCondition *>());
}

bool Instrument::watches(WatchedType type) const {
    if (type < 0 || type >= WatchedTypeCount) {
        qnWarning("Invalid watched type %1", type);
        return false;
    }

    return !m_watchedEventTypes[type].empty();
}

namespace {
    Q_GLOBAL_STATIC(Instrument::EventTypeSet, globalEmptyEventTypeSet);
}

const Instrument::EventTypeSet &Instrument::watchedEventTypes(WatchedType type) const {
    if (type < 0 || type >= WatchedTypeCount) {
        qnWarning("Invalid watched type %1", type);

        return *globalEmptyEventTypeSet();
    }

    return m_watchedEventTypes[type];
}

void Instrument::setItemConditions(const QList<InstrumentItemCondition *> &itemConditions) {
    QList<InstrumentItemCondition *> effectiveItemConditions = itemConditions;

    if(itemConditions.contains(NULL)) {
        qnWarning("Given item conditions list contains NULL conditions.");
        effectiveItemConditions.removeAll(NULL);
    }

    if(isInstalled()) {
        qnWarning("Cannot change item conditions of an installed instrument.");
        m_ownedItemConditions.unite(effectiveItemConditions.toSet()); /* We must claim ownership. */
        return;
    }

    qDeleteAll(m_ownedItemConditions);
    m_ownedItemConditions = effectiveItemConditions.toSet();
    m_itemConditions = effectiveItemConditions;
}

void Instrument::addItemCondition(InstrumentItemCondition *itemCondition) {
    if(!itemCondition) {
        qnNullWarning(itemCondition);
        return;
    }

    if(isInstalled()) {
        qnWarning("Cannot change item conditions of an installed instrument.");
        m_ownedItemConditions.insert(itemCondition); /* We must claim ownership. */
        return;
    }

    if(m_itemConditions.contains(itemCondition))
        return; /* Re-registration is allowed. */

    m_ownedItemConditions.insert(itemCondition);
    m_itemConditions.push_back(itemCondition);
}

void Instrument::removeItemCondition(InstrumentItemCondition *itemCondition) {
    if(!itemCondition) {
        qnNullWarning(itemCondition);
        return;
    }

    if(isInstalled()) {
        qnWarning("Cannot change item conditions of an installed instrument.");
        m_ownedItemConditions.remove(itemCondition); /* But we must release ownership nevertheless. */
        return;
    }

    m_ownedItemConditions.remove(itemCondition);
    m_itemConditions.removeAll(itemCondition);
}

bool Instrument::satisfiesItemConditions(QGraphicsItem *item) const {
    if(!item) {
        qnNullWarning(item);
        return false;
    }

    foreach(InstrumentItemCondition *condition, m_itemConditions)
        if(!condition->operator()(item, const_cast<Instrument *>(this)))
            return false;

    return true;
}

void Instrument::ensureUninstalled() {
    if (isInstalled())
        m_manager->uninstallInstrument(this);
}

QGraphicsView *Instrument::view(QWidget *viewport) {
    NX_ASSERT(viewport != NULL);

    return checked_cast<QGraphicsView *>(viewport->parent());
}

QList<QGraphicsItem *> Instrument::items(QGraphicsView *view, const QPoint &viewPos) const {
    return items(view, view->mapToScene(viewPos));
}

QList<QGraphicsItem *> Instrument::items(QGraphicsView *view, const QPointF &scenePos) const {
    if (m_scene == NULL) {
        if (!isInstalled()) {
            qnWarning("Instrument is not installed.");
        } else {
            qnWarning("Scene is being destroyed.");
        }

        return QList<QGraphicsItem *>();
    }

    /* Note that it would be more correct to use view->mapToScene(QRect(viewPos, QSize(1, 1))).
     * However, this is not the way it is implemented in Qt, so we have no other options but to mimic Qt behavior. */
    QRectF pointRect = QRectF(scenePos, QSizeF(1, 1));
    if (!view->isTransformed())
        return m_scene->items(pointRect, Qt::IntersectsItemShape, Qt::DescendingOrder);

    return m_scene->items(pointRect, Qt::IntersectsItemShape, Qt::DescendingOrder, view->viewportTransform());
}

QList<QGraphicsItem *> Instrument::items(const QGraphicsSceneMouseEvent *event) const {
    NX_ASSERT(event != NULL);

    if (m_scene == NULL) {
        if (!isInstalled()) {
            qnWarning("Instrument is not installed.");
        } else {
            qnWarning("Scene is being destroyed.");
        }

        return QList<QGraphicsItem *>();
    }

    /* It is not clear whether event->widget() may not be a graphics view's viewport,
     * so we use qobject_cast to feel safe. */
    QWidget *viewport = event->widget();
    QGraphicsView *view = viewport == NULL ? NULL : qobject_cast<QGraphicsView *>(viewport->parent());

    if(view == NULL) {
        return m_scene->items(event->scenePos(), Qt::IntersectsItemShape, Qt::DescendingOrder, QTransform());
    } else {
        return this->items(view, event->scenePos());
    }
}

QSet<QGraphicsView *> Instrument::views() const {
    if(isInstalled()) {
        return m_manager->views();
    } else {
        return QSet<QGraphicsView *>();
    }
}

void Instrument::adjustDisabledCounter(int amount) {
    int oldDisabledCounter = m_disabledCounter;
    int newDisabledCounter = m_disabledCounter + amount;
    if(newDisabledCounter < 0)
        newDisabledCounter = 0;

    if(oldDisabledCounter == 0 && newDisabledCounter > 0 && isInstalled())
        aboutToBeDisabledNotify();

    m_disabledCounter = newDisabledCounter;

    if(oldDisabledCounter > 0 && newDisabledCounter == 0 && isInstalled())
        enabledNotify();
}

void Instrument::setEnabled(bool enabled) {
    if(enabled) {
        adjustDisabledCounter(-m_disabledCounter);
    } else {
        adjustDisabledCounter(0x10000);
    }
}

bool Instrument::isEnabled() const {
    return m_disabledCounter == 0 && isInstalled();
}

void Instrument::enable() {
    setEnabled(true);
}

void Instrument::disable() {
    setEnabled(false);
}

void Instrument::recursiveEnable() {
    adjustDisabledCounter(-1);
}

void Instrument::recursiveDisable() {
    adjustDisabledCounter(1);
}

void Instrument::sendInstalledNotifications(bool installed) {
    NX_ASSERT(isInstalled()); /* Must be actually installed. */

    if(installed) {
        installedNotify();
        emit this->installed();
        if(isEnabled()) {
            enabledNotify();
            emit enabled();
        }
    } else {
        if(isEnabled()) {
            emit aboutToBeDisabled();
            aboutToBeDisabledNotify();
        }
        emit aboutToBeUninstalled();
        aboutToBeUninstalledNotify();
    }
}

bool Instrument::event(QGraphicsScene *watched, QEvent *event) {
    return dispatchEvent(watched, event);
}

bool Instrument::event(QGraphicsView *watched, QEvent *event) {
    return dispatchEvent(watched, event);
}

bool Instrument::event(QWidget *watched, QEvent *event) {
    return dispatchEvent(watched, event);
}

bool Instrument::sceneEvent(QGraphicsItem *watched, QEvent *event) {
    return dispatchEvent(watched, event);
}

template<class T>
bool Instrument::dispatchEvent(T *watched, QEvent *event) {
    switch ((int)event->type()) {
    case QEvent::MouseMove:
        return mouseMoveEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonPress:
        return mousePressEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonRelease:
        return mouseReleaseEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::MouseButtonDblClick:
        return mouseDoubleClickEvent(watched, static_cast<QMouseEvent *>(event));
    case QEvent::Wheel:
        return wheelEvent(watched, static_cast<QWheelEvent *>(event));
    case QEvent::TabletMove:
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
        return tabletEvent(watched, static_cast<QTabletEvent *>(event));
    case QEvent::KeyPress:
        return keyPressEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::KeyRelease:
        return keyReleaseEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::InputMethod:
        return inputMethodEvent(watched, static_cast<QInputMethodEvent *>(event));
    case QEvent::FocusIn:
        return focusInEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::FocusOut:
        return focusOutEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::Enter:
        return enterEvent(watched, static_cast<QEvent *>(event));
    case QEvent::Leave:
        return leaveEvent(watched, static_cast<QEvent *>(event));
    case QEvent::Paint:
        return paintEvent(watched, static_cast<QPaintEvent *>(event));
    case QEvent::Move:
        return moveEvent(watched, static_cast<QMoveEvent *>(event));
    case QEvent::Resize:
        return resizeEvent(watched, static_cast<QResizeEvent *>(event));
    case QEvent::Close:
        return closeEvent(watched, static_cast<QCloseEvent *>(event));
    case QEvent::ContextMenu:
        return contextMenuEvent(watched, static_cast<QContextMenuEvent *>(event));
    case QEvent::Drop:
        return dropEvent(watched, static_cast<QDropEvent *>(event));
    case QEvent::DragEnter:
        return dragEnterEvent(watched, static_cast<QDragEnterEvent *>(event));
    case QEvent::DragMove:
        return dragMoveEvent(watched, static_cast<QDragMoveEvent *>(event));
    case QEvent::DragLeave:
        return dragLeaveEvent(watched, static_cast<QDragLeaveEvent *>(event));
    case QEvent::Show:
        return showEvent(watched, static_cast<QShowEvent *>(event));
    case QEvent::Hide:
        return hideEvent(watched, static_cast<QHideEvent *>(event));
    case QEvent::ToolBarChange:
    case QEvent::ActivationChange:
    case QEvent::EnabledChange:
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::WindowTitleChange:
    case QEvent::IconTextChange:
    case QEvent::ModifiedChange:
    case QEvent::MouseTrackingChange:
    case QEvent::ParentChange:
    case QEvent::WindowStateChange:
    case QEvent::LocaleChange:
    case QEvent::MacSizeChange:
    case QEvent::ContentsRectChange:
    case QEvent::LanguageChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::KeyboardLayoutChange:
        return changeEvent(watched, static_cast<QEvent *>(event));
    case QEvent::ActionAdded:
    case QEvent::ActionRemoved:
    case QEvent::ActionChanged:
        return actionEvent(watched, static_cast<QActionEvent *>(event));
    case QEvent::GraphicsSceneDragEnter:
        return dragEnterEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragMove:
        return dragMoveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragLeave:
        return dragLeaveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDrop:
        return dropEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneContextMenu:
        return contextMenuEvent(watched, static_cast<QGraphicsSceneContextMenuEvent *>(event));
    case QEvent::GraphicsSceneMouseMove:
        return mouseMoveEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMousePress:
        return mousePressEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseRelease:
        return mouseReleaseEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseDoubleClick:
        return mouseDoubleClickEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneWheel:
        return wheelEvent(watched, static_cast<QGraphicsSceneWheelEvent *>(event));
    case QEvent::GraphicsSceneHelp:
        return helpEvent(watched, static_cast<QGraphicsSceneHelpEvent *>(event));
    case AnimationEvent::Animation: {
        AnimationEvent *e = static_cast<AnimationEvent *>(event);

        /* Automagic support for paint-synced animations. */
        if(m_animationTimer)
            m_animationTimer->updateCurrentTime(e->time());

        return animationEvent(e);
    }
    default:
        return false;
    }
}

bool Instrument::dispatchEvent(QGraphicsItem *watched, QEvent *event) {
    switch (event->type()) {
    case QEvent::GraphicsSceneContextMenu:
        return contextMenuEvent(watched, static_cast<QGraphicsSceneContextMenuEvent *>(event));
    case QEvent::GraphicsSceneDragEnter:
        return dragEnterEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragLeave:
        return dragLeaveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDragMove:
        return dragMoveEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::GraphicsSceneDrop:
        return dropEvent(watched, static_cast<QGraphicsSceneDragDropEvent *>(event));
    case QEvent::FocusIn:
        return focusInEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::FocusOut:
        return focusOutEvent(watched, static_cast<QFocusEvent *>(event));
    case QEvent::GraphicsSceneHoverEnter:
        return hoverEnterEvent(watched, static_cast<QGraphicsSceneHoverEvent *>(event));
    case QEvent::GraphicsSceneHoverLeave:
        return hoverLeaveEvent(watched, static_cast<QGraphicsSceneHoverEvent *>(event));
    case QEvent::GraphicsSceneHoverMove:
        return hoverMoveEvent(watched, static_cast<QGraphicsSceneHoverEvent *>(event));
    case QEvent::InputMethod:
        return inputMethodEvent(watched, static_cast<QInputMethodEvent *>(event));
    case QEvent::KeyPress:
        return keyPressEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::KeyRelease:
        return keyReleaseEvent(watched, static_cast<QKeyEvent *>(event));
    case QEvent::GraphicsSceneMouseDoubleClick:
        return mouseDoubleClickEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseMove:
        return mouseMoveEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMousePress:
        return mousePressEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneMouseRelease:
        return mouseReleaseEvent(watched, static_cast<QGraphicsSceneMouseEvent *>(event));
    case QEvent::GraphicsSceneWheel:
        return wheelEvent(watched, static_cast<QGraphicsSceneWheelEvent *>(event));
    default:
        return false;
    }
}

AnimationTimer *Instrument::animationTimer() {
    if(m_animationTimer.isNull())
        m_animationTimer.reset(new AnimationTimer());

    return m_animationTimer.data();
}
