#ifndef QN_INSTRUMENT_H
#define QN_INSTRUMENT_H

#include <functional> /* For std::unary_function. */
#include <QObject>
#include <QEvent> /* For QEvent::Type. */
#include <QSet>
#include <QList>
#include <utils/common/scene_utility.h>

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

class InstrumentManager;
class InstrumentItemEventDispatcher;
template<class T>
class InstrumentEventDispatcher;

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


class Instrument: public QObject, protected QnSceneUtility {
    Q_OBJECT;

public:
    /**
     * Enumeration for different types of objects that can be watched by an instrument.
     */
    enum WatchedType {
        SCENE,
        VIEW,
        VIEWPORT,
        ITEM,
        WATCHED_TYPE_COUNT
    };

    typedef QSet<QEvent::Type> EventTypeSet;

    /**
     * Constructor.
     * 
     * When installed, instrument will watch the given set of event types for 
     * each watched type. 
     * 
     * \param sceneEventTypes          Set of scene event types that this instrument watches.
     * \param viewEventTypes           Set of view event types that this instrument watches.
     * \param viewportEventTypes       Set of viewport event types that this instrument watches.
     * \param itemEventTypes           Set of item event types that this instrument watches.
     * \param parent                   Parent of this Instrument.
     *                                 Must not be NULL.
     */
    Instrument(const EventTypeSet &sceneEventTypes, const EventTypeSet &viewEventTypes, const EventTypeSet &viewportEventTypes, const EventTypeSet &itemEventTypes, QObject *parent);

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
     * \returns                        Graphics scene that this instrument is
     *                                 installed to, or NULL if none.
     */
    QGraphicsScene *scene() const {
        return m_scene;
    }

    /**
     * \returns                        Set of all graphics views that this 
     *                                 instrument watches.
     */
    QSet<QGraphicsView *> views() const;

    /**
     * Disables or enables this instrument. Note that disabled instruments do
     * not receive notifications and cannot filter events.
     *
     * \param enabled                  Whether this instrument is enabled.
     */
    void setEnabled(bool enabled = true);

    /**
     * \returns                        Whether this instrument is enabled.
     */
    bool isEnabled() const;

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

protected:
    friend class InstrumentManagerPrivate; /* Messes with our internals. */
    template<class T>
    friend class InstrumentEventDispatcher; /* Calls event handlers and isWillingToWatch. */


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

    /**
     * Extension point for instrument installation.
     * 
     * It is guaranteed that scene will not be NULL inside this call.
     */
    virtual void installedNotify() {}

    /**
     * Extension point for instrument uninstallation.
     * 
     * Scene may be NULL inside this call, which would mean that scene is being
     * destroyed.
     */
    virtual void aboutToBeUninstalledNotify() {}

    /**
     * Extension point for instrument enabling.
     */
    virtual void enabledNotify() {}

    /**
     * Extension point for instrument disabling.
     */
    virtual void aboutToBeDisabledNotify() {}

    /**
     * \param item                     Graphics item.
     * \returns                        Whether this instrument is willing to watch
     *                                 events of the given graphics item.
     */
    virtual bool isWillingToWatch(QGraphicsItem *) const { return true; }

    /**
     * \param view                     Graphics view.
     * \returns                        Whether this instrument is willing to watch
     *                                 events of the given graphics view.
     */
    virtual bool isWillingToWatch(QGraphicsView *) const { return true; }

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
    QList<QGraphicsItem *> items(QGraphicsView *view, const QPoint &viewPos) const;
    QList<QGraphicsItem *> items(const QGraphicsSceneMouseEvent *event) const;

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

private:
    InstrumentManager *m_manager;
    QGraphicsScene *m_scene;
    EventTypeSet m_watchedEventTypes[WATCHED_TYPE_COUNT];
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

QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QGraphicsScene, SCENE);
QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QGraphicsView,  VIEW);
QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QWidget,        VIEWPORT);
QN_DEFINE_INSTRUMENT_WATCHED_TYPE(QGraphicsItem,  ITEM);

#undef QN_DEFINE_INSTRUMENT_WATCHED_TYPE

#endif // QN_INSTRUMENT_H
