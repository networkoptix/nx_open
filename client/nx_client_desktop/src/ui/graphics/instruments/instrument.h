#pragma once

#include <functional> /* For std::unary_function. */

#include <QtCore/QScopedPointer>
#include <QtCore/QObject>
#include <QtCore/QEvent> /* For QEvent::Type. */
#include <QtCore/QSet>
#include <QtCore/QList>

#include <utils/common/connective.h>

#include <ui/common/scene_transformations.h>
#include <ui/customization/customized.h>

#include "instrument_item_condition.h"

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QTabletEvent;
class QInputMethodEvent;
class QFocusEvent;
class QPaintEvent;
class QMoveEvent;
class QResizeEvent;
class QCloseEvent;
class QContextMenuEvent;
class QDropEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QShowEvent;
class QHideEvent;
class QActionEvent;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneWheelEvent;
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneHelpEvent;
class QGraphicsSceneHoverEvent;

class QWidget;
class QGraphicsView;
class QGraphicsItem;
class QGraphicsScene;
class QPoint;

class AnimationTimer;
class AnimationEvent;
class InstrumentManager;
class InstrumentItemEventDispatcher;
template<class T>
class InstrumentEventDispatcher;
class InstrumentItemCondition;

namespace detail {
    struct AlwaysTrue: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *) const {
            return true;
        }
    };

    template<class GraphicsItem, class Condition>
    struct CompoundCondition: public std::unary_function<QGraphicsItem *, bool> {
        CompoundCondition(const Condition &condition): condition(condition) {}

        bool operator()(QGraphicsItem *item) const {
            return condition(item) && dynamic_cast<GraphicsItem *>(item) != NULL;
        }

        const Condition &condition;
    };

} // namespace detail


/**
 * <tt>Instrument</tt> is a base class for detached event processing on a graphics scene,
 * its views and items.
 *
 * Each instrument supplies a set of event types that it is willing to watch
 * for each of the watched types. These sets are supplied through the
 * <tt>Instrument</tt>'s constructor.
 *
 * As addition and removal of graphics items to and from the scene cannot be
 * automatically detected, items must either inherit from <tt>Instrumented</tt>
 * class, or be registered and unregistered manually.
 *
 * Scene and its views must always be registered manually. They will be
 * unregistered upon destruction, so there is no need to unregister them
 * manually.
 *
 * Instrument provides several extension points for the derived classes to
 * track its current state. Instrument state consists of:
 * <ul>
 * <li>Installed bit, accessed via <tt>isInstalled()</tt> function.
 *     If not installed, instrument does not receive events and has no
 *     associated scene, views or items.
 *     Derived classes can track changes of this bit by reimplementing
 *     <tt>installedNotify()</tt> and <tt>aboutToBeUninstalledNotify()</tt> functions. </li>
 * <li>Enabled bit, accessed via <tt>isEnabled()</tt> function.
 *     If not enabled, instrument does not receive events. However, it still
 *     can access its associated scene, views and items.
 *     Derived classes can track changes of this bit by reimplementing
 *     <tt>enabledNotify()</tt> and <tt>aboutToBeDisabledNotify()</tt> functions. </li>
 * </ul>
 *
 * Note that when uninstalled, instrument is always considered disabled. Upon
 * installation it restores its enabled state. This means that if the instrument
 * is installed and then uninstalled, the following notification functions will
 * be called:
 * <ol>
 * <li><tt>installedNotify()</tt></li>
 * <li><tt>enabledNotify()</tt></li>
 * <li><tt>aboutToBeDisabledNotify()</tt></li>
 * <li><tt>aboutToBeUninstalledNotify()</tt></li>
 * <ol>
 *
 * This is why in most cases it is enough for the derived class to
 * reimplement only <tt>aboutToBeDisabledNotify()</tt> function.
 *
 * Note that it is a <b>must</b> to call <tt>ensureUninstalled()</tt> function
 * inside derived class's destructor if it reimplements either
 * <tt>aboutToBeDisabledNotify()</tt> or <tt>aboutToBeUninstalledNotify()</tt>.
 */
class Instrument: public Customized<Connective<QObject>>, protected QnSceneTransformations
{
    Q_OBJECT

    using base_type = Customized<Connective<QObject>>;

public:
    /**
     * Enumeration for different types of objects that can be watched by an instrument.
     */
    enum WatchedType {
        Viewport,      /**< Viewport. */
        View,          /**< Graphics view. */
        Scene,         /**< Graphics scene. */
        Item,          /**< Graphics item. */
        WatchedTypeCount
    };

    typedef QSet<QEvent::Type> EventTypeSet;

    /**
     * Constructor.
     *
     * When installed, instrument will watch the given set of event types for
     * each watched type.
     *
     * \param viewportEventTypes       Set of viewport event types that this instrument watches.
     * \param viewEventTypes           Set of view event types that this instrument watches.
     * \param sceneEventTypes          Set of scene event types that this instrument watches.
     * \param itemEventTypes           Set of item event types that this instrument watches.
     * \param parent                   Parent of this instrument.
     */
    Instrument(const EventTypeSet &viewportEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &sceneEventTypes, const EventTypeSet &itemEventTypes, QObject *parent);

    /**
     * Constructor.
     *
     * \param type                     Type of the objects that this instrument watches.
     * \param eventTypes               Set of event types that this instrument watches.
     * \param parent                   Parent of this instrument.
     */
    Instrument(WatchedType type, const EventTypeSet &eventTypes, QObject *parent);

    /**
     * Virtual destructor.
     */
    virtual ~Instrument();

    /**
     * \param type                     Type of an object that can be watched by this instrument.
     * \returns                        Whether this instrument watches the given type.
     */
    bool watches(WatchedType type) const;

    /**
     * \param type                     Type of an object that can be watched by this instrument.
     * \returns                        Set of event types for the given watched
     *                                 type that this instrument watches.
     */
    const EventTypeSet &watchedEventTypes(WatchedType type) const;

    /**
     * \returns                        Whether this instrument is installed.
     */
    bool isInstalled() const {
        return m_manager != NULL;
    }

    /**
     * \returns                        Instrument manager for this instrument, or NULL if none.
     */
    InstrumentManager *manager() const {
        return m_manager;
    }

    /**
     * Note that this function returns NULL if this instrument isn't installed to
     * any scene, or if the scene that it is installed to is being destroyed.
     *
     * \returns                        Graphics scene that this instrument is installed to.
     */
    QGraphicsScene *scene() const;

    void setScene(QGraphicsScene* scene);

    /**
     * \returns                        Set of all graphics views that this
     *                                 instrument watches.
     */
    QSet<QGraphicsView *> views() const;

    /**
     * Disables or enables this instrument. Note that disabled instruments
     * cannot filter events.
     *
     * \param enabled                  Whether this instrument is enabled.
     */
    void setEnabled(bool enabled = true);

    /**
     * \returns                        Whether this instrument is enabled.
     */
    bool isEnabled() const;

    /**
     * \returns                        List of item conditions of this instrument.
     */
    const QList<InstrumentItemCondition *> &itemConditions() const {
        return m_itemConditions;
    }

    /**
     * This function sets item conditions of the instrument. Note that once
     * instrument is installed, item conditions cannot be changed.
     *
     * Instrument takes ownership of the supplied item conditions.
     *
     * \param itemConditions           New item conditions for this instrument.
     */
    void setItemConditions(const QList<InstrumentItemCondition *> &itemConditions);

    /**
     * This function registers additional item condition with this instrument.
     * Note that once instrument is installed, new item conditions cannot be added.
     *
     * Instrument takes ownership of the supplied item condition.
     *
     * \param itemCondition            Item condition to add to this instrument's item conditions list.
     */
    void addItemCondition(InstrumentItemCondition *itemCondition);

    /**
     * This function unregisters an item condition with this instrument.
     * Note that once instrument is installed, new item conditions cannot be added.
     *
     * \param itemCondition            Item condition to remove from this instrument's item condition list.
     */
    void removeItemCondition(InstrumentItemCondition *itemCondition);

    /**
     * \param item                      Item to check.
     * \returns                         Whether the given item satisfies all of this instrument's item conditions.
     */
    bool satisfiesItemConditions(QGraphicsItem *item) const;

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \returns                        Empty event type set.
     */
    static EventTypeSet makeSet() {
        return EventTypeSet();
    }

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventType                Event type.
     * \returns                        Set constructed from the given event type.
     */
    static EventTypeSet makeSet(QEvent::Type eventType) {
        EventTypeSet result;
        result.insert(eventType);
        return result;
    }

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventType0               First event type.
     * \param eventType1               Second event type.
     * \returns                        Set constructed from the given event types.
     */
    static EventTypeSet makeSet(QEvent::Type eventType0, QEvent::Type eventType1) {
        EventTypeSet result;
        result.insert(eventType0);
        result.insert(eventType1);
        return result;
    }

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventType0               First event type.
     * \param eventType1               Second event type.
     * \param eventType2               Third event type.
     * \returns                        Set constructed from the given event types.
     */
    static EventTypeSet makeSet(QEvent::Type eventType0, QEvent::Type eventType1, QEvent::Type eventType2) {
        EventTypeSet result;
        result.insert(eventType0);
        result.insert(eventType1);
        result.insert(eventType2);
        return result;
    }

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventType0               First event type.
     * \param eventType1               Second event type.
     * \param eventType2               Third event type.
     * \param eventType3               Fourth event type.
     * \returns                        Set constructed from the given event types.
     */
    static EventTypeSet makeSet(QEvent::Type eventType0, QEvent::Type eventType1, QEvent::Type eventType2, QEvent::Type eventType3) {
        EventTypeSet result;
        result.insert(eventType0);
        result.insert(eventType1);
        result.insert(eventType2);
        result.insert(eventType3);
        return result;
    }

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventType0               First event type.
     * \param eventType1               Second event type.
     * \param eventType2               Third event type.
     * \param eventType3               Fourth event type.
     * \param eventType4               Fifth event type.
     * \returns                        Set constructed from the given event types.
     */
    static EventTypeSet makeSet(QEvent::Type eventType0, QEvent::Type eventType1, QEvent::Type eventType2, QEvent::Type eventType3, QEvent::Type eventType4) {
        EventTypeSet result;
        result.insert(eventType0);
        result.insert(eventType1);
        result.insert(eventType2);
        result.insert(eventType3);
        result.insert(eventType4);
        return result;
    }

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventType0               First event type.
     * \param eventType1               Second event type.
     * \param eventType2               Third event type.
     * \param eventType3               Fourth event type.
     * \param eventType4               Fifth event type.
     * \param eventType5               Sixth event type.
     * \returns                        Set constructed from the given event types.
     */
    static EventTypeSet makeSet(QEvent::Type eventType0, QEvent::Type eventType1, QEvent::Type eventType2, QEvent::Type eventType3, QEvent::Type eventType4, QEvent::Type eventType5) {
        EventTypeSet result;
        result.insert(eventType0);
        result.insert(eventType1);
        result.insert(eventType2);
        result.insert(eventType3);
        result.insert(eventType4);
        result.insert(eventType5);
        return result;
    }

    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventType0               First event type.
     * \param eventType1               Second event type.
     * \param eventType2               Third event type.
     * \param eventType3               Fourth event type.
     * \param eventType4               Fifth event type.
     * \param eventType5               Sixth event type.
     * \param eventType6               Seventh event type.
     * \returns                        Set constructed from the given event types.
     */
    static EventTypeSet makeSet(QEvent::Type eventType0, QEvent::Type eventType1, QEvent::Type eventType2, QEvent::Type eventType3, QEvent::Type eventType4, QEvent::Type eventType5, QEvent::Type eventType6) {
        EventTypeSet result;
        result.insert(eventType0);
        result.insert(eventType1);
        result.insert(eventType2);
        result.insert(eventType3);
        result.insert(eventType4);
        result.insert(eventType5);
        result.insert(eventType6);
        return result;
    }


    /**
     * This function is to be used in constructors of derived classes.
     *
     * \param eventTypes               Array of event types.
     * \returns                        Set constructed from the given event types.
     */
    template<int N>
    static EventTypeSet makeSet(const QEvent::Type (&eventTypes)[N]) {
        EventTypeSet result;
        for (int i = 0; i < N; i++)
            result.insert(eventTypes[i]);
        return result;
    }

public slots:
    /**
     * Enables this instrument.
     */
    void enable();

    /**
     * Disables this instrument.
     */
    void disable();

    /**
     * Recursively enables this instrument. If it was recursively disabled N times,
     * then it must be recursively re-enabled N times before it would become truly
     * enabled.
     */
    void recursiveEnable();

    /**
     * Recursively disables this instrument.
     */
    void recursiveDisable();

signals:
    /**
     * This signal is emitted whenever the instrument is installed.
     */
    void installed();

    /**
     * This signal is emitted whenever the instrument is about to be uninstalled.
     */
    void aboutToBeUninstalled();

    /**
     * This signal is emitted whenever the instrument is enabled.
     */
    void enabled();

    /**
     * This signal is emitted whenever the instrument is disabled.
     */
    void aboutToBeDisabled();

    void sceneChanged();

protected:
    friend class InstrumentManagerPrivate; /* Messes with our internals. */
    template<class T>
    friend class InstrumentEventDispatcher; /* Calls event handlers and itemRegisteredNotify. */
    friend class RedirectingInstrument; /* Calls event handlers. */

    /**
     * Ensure that this instrument is uninstalled.
     *
     * This method is to be called from destructor of the derived class if
     * it preforms some complex actions during uninstallation.
     * This is because derived implementation of the <tt>aboutToBeUninstalled</tt>
     * extension point cannot be called from the destructor of the base class.
     */
    void ensureUninstalled();

    /**
     * \param viewport                 Viewport. Must not be NULL and must actually be a viewport.
     * \returns                        Graphics view of the given viewport.
     */
    static QGraphicsView *view(QWidget *viewport);

    /**
     * \param view                     Current view. Used to handle items that
     *                                 ignore transformations.
     * \param viewPos                  Position in view coordinates to get item at.
     * \param condition                Condition that returned item should satisfy.
     * \returns                        Topmost item at the given position that
     *                                 satisfies the given condition.
     */
    template<class Condition>
    QGraphicsItem *item(QGraphicsView *view, const QPoint &viewPos, const Condition &condition) const {
        return this->item(this->items(view, viewPos), condition);
    }

    /**
     * \param event                    Graphics scene mouse event.
     * \param condition                Condition that returned item should satisfy.
     * \returns                        Topmost item at position determined by given
     *                                 mouse event that satisfies the given condition.
     */
    template<class Condition>
    QGraphicsItem *item(const QGraphicsSceneMouseEvent *event, const Condition &condition) const {
        return this->item(this->items(event), condition);
    }

    /**
     * \param view                     Current view. Used to handle items that
     *                                 ignore transformations.
     * \param viewPos                  Position in view coordinates to get item at.
     * \returns                        Topmost item at the given position.
     */
    QGraphicsItem *item(QGraphicsView *view, const QPoint &viewPos) const {
        return this->item(view, viewPos, detail::AlwaysTrue());
    }

    /**
     * \tparam GraphicsItem            Type of the items to look for.
     * \param view                     Current view. Used to handle items that
     *                                 ignore transformations.
     * \param viewPos                  Position in view coordinates to get item at.
     * \param condition                Condition that returned item should satisfy.
     * \returns                        Topmost item of the given type at the given
     *                                 position that satisfies the given condition.
     */
    template<class GraphicsItem, class Condition>
    GraphicsItem *item(QGraphicsView *view, const QPoint &viewPos, const Condition &condition) const {
        return static_cast<GraphicsItem *>(this->item(view, viewPos, detail::CompoundCondition<GraphicsItem, Condition>(condition)));
    }

    /**
     * \tparam GraphicsItem            Type of the items to look for.
     * \param view                     Current view. Used to handle items that
     *                                 ignore transformations.
     * \param viewPos                  Position in view coordinates to get item at.
     * \returns                        Topmost item of the given type at the given
     *                                 position.
     */
    template<class GraphicsItem>
    GraphicsItem *item(QGraphicsView *view, const QPoint &viewPos) const {
        return this->item<GraphicsItem>(view, viewPos, detail::AlwaysTrue());
    }

    QList<QGraphicsItem *> items(QGraphicsView *view, const QPoint &viewPos) const;
    QList<QGraphicsItem *> items(QGraphicsView *view, const QPointF &scenePos) const;
    QList<QGraphicsItem *> items(const QGraphicsSceneMouseEvent *event) const;

    /**
     * For the returned timer to be functional, the instrument must be
     * subscribed to <tt>Animation</tt> viewport event.
     *
     * Note that if the instrument is disabled, it won't receive the <tt>Animation</tt>
     * events, and thus the timer won't work.
     *
     * \returns                         Animation timer for this instrument that is
     *                                  synced with paint events.
     */
    AnimationTimer *animationTimer();

    /**
     * Extension point for instrument enabling.
     */
    virtual void enabledNotify() {}

    /**
     * Extension point for instrument disabling.
     *
     * Scene may be NULL inside this call, which would mean that scene is being
     * destroyed.
     */
    virtual void aboutToBeDisabledNotify() {}

    /**
     * Extension point for instrument installation.
     *
     * It is guaranteed that scene will not be NULL inside this call.
     *
     * Note that <tt>enabledNotify()</tt> will be called right after this call.
     */
    virtual void installedNotify() {}

    /**
     * Extension point for instrument uninstallation.
     *
     * Scene may be NULL inside this call, which would mean that scene is being
     * destroyed.
     *
     * Note that </tt>aboutToBeDisabledNotify()</tt> will be called right before this call.
     */
    virtual void aboutToBeUninstalledNotify() {}

    /**
     * \param viewport                 Viewport of a graphics view.
     * \returns                        Whether this instrument is willing to watch
     *                                 events of the given viewport.
     */
    virtual bool registeredNotify(QWidget *viewport) { Q_UNUSED(viewport); return true; }

    /**
     * \param viewport                 Viewport that was previously registered with this instrument.
     */
    virtual void unregisteredNotify(QWidget *viewport) { Q_UNUSED(viewport); }

    /**
     * \param view                     Graphics view.
     * \returns                        Whether this instrument is willing to watch
     *                                 events of the given graphics view.
     */
    virtual bool registeredNotify(QGraphicsView *view) { Q_UNUSED(view); return true; }

    /**
     * \param view                     Graphics view that was previously registered with this instrument.
     */
    virtual void unregisteredNotify(QGraphicsView *view) { Q_UNUSED(view); }

    /**
     * \param item                     Graphics item.
     * \returns                        Whether this instrument is willing to watch
     *                                 events of the given graphics item.
     */
    virtual bool registeredNotify(QGraphicsItem *item) { return satisfiesItemConditions(item); }

    /**
     * \param item                     Graphics item that was previously registered with this instrument.
     */
    virtual void unregisteredNotify(QGraphicsItem *item) { Q_UNUSED(item); }

    /* Graphics view's viewport event filtering functions. */
    virtual bool event(QWidget *, QEvent *);
    virtual bool mouseMoveEvent(QWidget *, QMouseEvent *) { return false; }
    virtual bool mousePressEvent(QWidget *, QMouseEvent *) { return false; }
    virtual bool mouseReleaseEvent(QWidget *, QMouseEvent *) { return false; }
    virtual bool mouseDoubleClickEvent(QWidget *, QMouseEvent *) { return false; }
    virtual bool wheelEvent(QWidget *, QWheelEvent *) { return false; }
    virtual bool tabletEvent(QWidget *, QTabletEvent *) { return false; }
    virtual bool keyPressEvent(QWidget *, QKeyEvent *) { return false; }
    virtual bool keyReleaseEvent(QWidget *, QKeyEvent *) { return false; }
    virtual bool inputMethodEvent(QWidget *, QInputMethodEvent *) { return false; }
    virtual bool focusInEvent(QWidget *, QFocusEvent *) { return false; }
    virtual bool focusOutEvent(QWidget *, QFocusEvent *) { return false; }
    virtual bool enterEvent(QWidget *, QEvent *) { return false; }
    virtual bool leaveEvent(QWidget *, QEvent *) { return false; }
    virtual bool paintEvent(QWidget *, QPaintEvent *) { return false; }
    virtual bool moveEvent(QWidget *, QMoveEvent *) { return false; }
    virtual bool resizeEvent(QWidget *, QResizeEvent *) { return false; }
    virtual bool closeEvent(QWidget *, QCloseEvent *) { return false; }
    virtual bool contextMenuEvent(QWidget *, QContextMenuEvent *) { return false; }
    virtual bool dropEvent(QWidget *, QDropEvent *) { return false; }
    virtual bool dragEnterEvent(QWidget *, QDragEnterEvent *) { return false; }
    virtual bool dragMoveEvent(QWidget *, QDragMoveEvent *) { return false; }
    virtual bool dragLeaveEvent(QWidget *, QDragLeaveEvent *) { return false; }
    virtual bool showEvent(QWidget *, QShowEvent *) { return false; }
    virtual bool hideEvent(QWidget *, QHideEvent *) { return false; }
    virtual bool changeEvent(QWidget *, QEvent *) { return false; }
    virtual bool actionEvent(QWidget *, QActionEvent *) { return false; }
    virtual bool dragEnterEvent(QWidget *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragMoveEvent(QWidget *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragLeaveEvent(QWidget *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dropEvent(QWidget *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool contextMenuEvent(QWidget *, QGraphicsSceneContextMenuEvent *) { return false; }
    virtual bool mouseMoveEvent(QWidget *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mousePressEvent(QWidget *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseReleaseEvent(QWidget *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseDoubleClickEvent(QWidget *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool wheelEvent(QWidget *, QGraphicsSceneWheelEvent *) { return false; }
    virtual bool helpEvent(QWidget *, QGraphicsSceneHelpEvent *) { return false; }

    /* Global animation event. Considered a viewport event. */
    virtual bool animationEvent(AnimationEvent *) { return false; }

    /* Graphics view event filtering functions. */
    virtual bool event(QGraphicsView *, QEvent *);
    virtual bool mouseMoveEvent(QGraphicsView *, QMouseEvent *) { return false; }
    virtual bool mousePressEvent(QGraphicsView *, QMouseEvent *) { return false; }
    virtual bool mouseReleaseEvent(QGraphicsView *, QMouseEvent *) { return false; }
    virtual bool mouseDoubleClickEvent(QGraphicsView *, QMouseEvent *) { return false; }
    virtual bool wheelEvent(QGraphicsView *, QWheelEvent *) { return false; }
    virtual bool tabletEvent(QGraphicsView *, QTabletEvent *) { return false; }
    virtual bool keyPressEvent(QGraphicsView *, QKeyEvent *) { return false; }
    virtual bool keyReleaseEvent(QGraphicsView *, QKeyEvent *) { return false; }
    virtual bool inputMethodEvent(QGraphicsView *, QInputMethodEvent *) { return false; }
    virtual bool focusInEvent(QGraphicsView *, QFocusEvent *) { return false; }
    virtual bool focusOutEvent(QGraphicsView *, QFocusEvent *) { return false; }
    virtual bool enterEvent(QGraphicsView *, QEvent *) { return false; }
    virtual bool leaveEvent(QGraphicsView *, QEvent *) { return false; }
    virtual bool paintEvent(QGraphicsView *, QPaintEvent *) { return false; }
    virtual bool moveEvent(QGraphicsView *, QMoveEvent *) { return false; }
    virtual bool resizeEvent(QGraphicsView *, QResizeEvent *) { return false; }
    virtual bool closeEvent(QGraphicsView *, QCloseEvent *) { return false; }
    virtual bool contextMenuEvent(QGraphicsView *, QContextMenuEvent *) { return false; }
    virtual bool dropEvent(QGraphicsView *, QDropEvent *) { return false; }
    virtual bool dragEnterEvent(QGraphicsView *, QDragEnterEvent *) { return false; }
    virtual bool dragMoveEvent(QGraphicsView *, QDragMoveEvent *) { return false; }
    virtual bool dragLeaveEvent(QGraphicsView *, QDragLeaveEvent *) { return false; }
    virtual bool showEvent(QGraphicsView *, QShowEvent *) { return false; }
    virtual bool hideEvent(QGraphicsView *, QHideEvent *) { return false; }
    virtual bool changeEvent(QGraphicsView *, QEvent *) { return false; }
    virtual bool actionEvent(QGraphicsView *, QActionEvent *) { return false; }
    virtual bool dragEnterEvent(QGraphicsView *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragMoveEvent(QGraphicsView *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragLeaveEvent(QGraphicsView *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dropEvent(QGraphicsView *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool contextMenuEvent(QGraphicsView *, QGraphicsSceneContextMenuEvent *) { return false; }
    virtual bool mouseMoveEvent(QGraphicsView *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mousePressEvent(QGraphicsView *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseReleaseEvent(QGraphicsView *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseDoubleClickEvent(QGraphicsView *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool wheelEvent(QGraphicsView *, QGraphicsSceneWheelEvent *) { return false; }
    virtual bool helpEvent(QGraphicsView *, QGraphicsSceneHelpEvent *) { return false; }

    /* Graphics scene event filtering functions. */
    virtual bool event(QGraphicsScene *, QEvent *);
    virtual bool mouseMoveEvent(QGraphicsScene *, QMouseEvent *) { return false; }
    virtual bool mousePressEvent(QGraphicsScene *, QMouseEvent *) { return false; }
    virtual bool mouseReleaseEvent(QGraphicsScene *, QMouseEvent *) { return false; }
    virtual bool mouseDoubleClickEvent(QGraphicsScene *, QMouseEvent *) { return false; }
    virtual bool wheelEvent(QGraphicsScene *, QWheelEvent *) { return false; }
    virtual bool tabletEvent(QGraphicsScene *, QTabletEvent *) { return false; }
    virtual bool keyPressEvent(QGraphicsScene *, QKeyEvent *) { return false; }
    virtual bool keyReleaseEvent(QGraphicsScene *, QKeyEvent *) { return false; }
    virtual bool inputMethodEvent(QGraphicsScene *, QInputMethodEvent *) { return false; }
    virtual bool focusInEvent(QGraphicsScene *, QFocusEvent *) { return false; }
    virtual bool focusOutEvent(QGraphicsScene *, QFocusEvent *) { return false; }
    virtual bool enterEvent(QGraphicsScene *, QEvent *) { return false; }
    virtual bool leaveEvent(QGraphicsScene *, QEvent *) { return false; }
    virtual bool paintEvent(QGraphicsScene *, QPaintEvent *) { return false; }
    virtual bool moveEvent(QGraphicsScene *, QMoveEvent *) { return false; }
    virtual bool resizeEvent(QGraphicsScene *, QResizeEvent *) { return false; }
    virtual bool closeEvent(QGraphicsScene *, QCloseEvent *) { return false; }
    virtual bool contextMenuEvent(QGraphicsScene *, QContextMenuEvent *) { return false; }
    virtual bool dropEvent(QGraphicsScene *, QDropEvent *) { return false; }
    virtual bool dragEnterEvent(QGraphicsScene *, QDragEnterEvent *) { return false; }
    virtual bool dragMoveEvent(QGraphicsScene *, QDragMoveEvent *) { return false; }
    virtual bool dragLeaveEvent(QGraphicsScene *, QDragLeaveEvent *) { return false; }
    virtual bool showEvent(QGraphicsScene *, QShowEvent *) { return false; }
    virtual bool hideEvent(QGraphicsScene *, QHideEvent *) { return false; }
    virtual bool changeEvent(QGraphicsScene *, QEvent *) { return false; }
    virtual bool actionEvent(QGraphicsScene *, QActionEvent *) { return false; }
    virtual bool dragEnterEvent(QGraphicsScene *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragMoveEvent(QGraphicsScene *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragLeaveEvent(QGraphicsScene *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dropEvent(QGraphicsScene *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool contextMenuEvent(QGraphicsScene *, QGraphicsSceneContextMenuEvent *) { return false; }
    virtual bool mouseMoveEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mousePressEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseReleaseEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseDoubleClickEvent(QGraphicsScene *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool wheelEvent(QGraphicsScene *, QGraphicsSceneWheelEvent *) { return false; }
    virtual bool helpEvent(QGraphicsScene *, QGraphicsSceneHelpEvent *) { return false; }

    /* Graphics item event filtering functions. */
    virtual bool sceneEvent(QGraphicsItem *, QEvent *);
    virtual bool contextMenuEvent(QGraphicsItem *, QGraphicsSceneContextMenuEvent *) { return false; }
    virtual bool dragEnterEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragLeaveEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dragMoveEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool dropEvent(QGraphicsItem *, QGraphicsSceneDragDropEvent *) { return false; }
    virtual bool focusInEvent(QGraphicsItem *, QFocusEvent *) { return false; }
    virtual bool focusOutEvent(QGraphicsItem *, QFocusEvent *) { return false; }
    virtual bool hoverEnterEvent(QGraphicsItem *, QGraphicsSceneHoverEvent *) { return false; }
    virtual bool hoverLeaveEvent(QGraphicsItem *, QGraphicsSceneHoverEvent *) { return false; }
    virtual bool hoverMoveEvent(QGraphicsItem *, QGraphicsSceneHoverEvent *) { return false; }
    virtual bool inputMethodEvent(QGraphicsItem *, QInputMethodEvent *) { return false; }
    virtual bool keyPressEvent(QGraphicsItem *, QKeyEvent *) { return false; }
    virtual bool keyReleaseEvent(QGraphicsItem *, QKeyEvent *) { return false; }
    virtual bool mouseDoubleClickEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseMoveEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mousePressEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool mouseReleaseEvent(QGraphicsItem *, QGraphicsSceneMouseEvent *) { return false; }
    virtual bool wheelEvent(QGraphicsItem *, QGraphicsSceneWheelEvent *) { return false; }

private:
    void initialize();

    template<class Condition>
    QGraphicsItem *item(const QList<QGraphicsItem *> &items, const Condition &condition) const {
        for (QList<QGraphicsItem *>::const_iterator i = items.begin(); i != items.end(); i++)
            if (condition(*i))
                return *i;

        return NULL;
    }

    template<class T>
    bool dispatchEvent(T *watched, QEvent *event);

    bool dispatchEvent(QGraphicsItem *watched, QEvent *event);

    void adjustDisabledCounter(int amount);

    void sendInstalledNotifications(bool installed);

private:
    InstrumentManager *m_manager;
    QGraphicsScene *m_scene;
    QList<InstrumentItemCondition *> m_itemConditions;
    QSet<InstrumentItemCondition *> m_ownedItemConditions;
    QScopedPointer<AnimationTimer> m_animationTimer;
    EventTypeSet m_watchedEventTypes[WatchedTypeCount];
    int m_disabledCounter;
};


/**
 * This metafunction maps a given type T to <tt>Instrument::WatchedType</tt> constant.
 */
template<class T>
struct instrument_watched_type;

#define QN_DEFINE_INSTRUMENT_WATCHED_TYPE(CLASS, TYPE)                          \
template<>                                                                      \
struct instrument_watched_type<CLASS> {                                         \
    static const Instrument::WatchedType value = Instrument::TYPE;              \
};                                                                              \

QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QGraphicsScene, Scene);
QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QGraphicsView,  View);
QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QWidget,        Viewport);
QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QGraphicsItem,  Item);

#undef QN_DEFINE_INSTRUMENT_WATCHED_TYPE
