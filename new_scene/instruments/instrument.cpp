#include "instrument.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTabletEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QActionEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsSceneHelpEvent>
#include <QGraphicsSceneHoverEvent>
#include <utility/Warnings.h>
#include <utility/CheckedCast.h>
#include "instrumentmanager.h"


Instrument::Instrument(const QSet<QEvent::Type> &sceneEventTypes, const QSet<QEvent::Type> &viewEventTypes, const QSet<QEvent::Type> &viewportEventTypes, bool watchesItemEvents, QObject *parent):
    QObject(parent),
    m_manager(NULL),
    m_scene(NULL),
    m_watchesItemEvents(watchesItemEvents),
    m_enabled(true)
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

void Instrument::setEnabled(bool enabled) {
    if(m_enabled == enabled)
        return;

    if(m_enabled)
        aboutToBeDisabledNotify();

    m_enabled = enabled;

    if(m_enabled)
        enabledNotify();
}

bool Instrument::isEnabled() const {
    return m_enabled;
}

void Instrument::enable() {
    setEnabled(true);
}

void Instrument::disable() {
    setEnabled(false);
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
    switch (event->type()) {
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
