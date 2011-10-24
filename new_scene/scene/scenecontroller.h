#ifndef QN_SCENE_CONTROLLER_H
#define QN_SCENE_CONTROLLER_H

#include <QObject>
#include <QRect>
#include <instruments/instrumentutility.h>

class QGraphicsScene;
class QGraphicsView;
class QGraphicsWidget;
class QGraphicsItem;
class QPoint;
class QRectF;
class QRect;

class CellLayout;
class AnimatedWidget;
class CentralWidget;
class InstrumentManager;

class BoundingInstrument;

class SceneController: public QObject, protected InstrumentUtility {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING,
        EDITING
    };

    SceneController(QObject *parent = NULL);

    void addView(QGraphicsView *view);

    QGraphicsScene *scene() const;

    Mode mode() const;

    void setMode(Mode mode);

protected:
    void relayoutItems(int rowCount, int columnCount, const QByteArray &preset);

    void invalidateLayout();

    void repositionFocusedWidget(QGraphicsView *view);

private slots:
    void at_relayoutAction_triggered();
    void at_centralWidget_geometryChanged();

    void at_widget_resizingStarted();
    void at_widget_resizingFinished();

    void at_draggingStarted(QGraphicsView *view, QList<QGraphicsItem *> items);
    void at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items);

    void at_clicked(QGraphicsView *view, QGraphicsItem *item);
    void at_doubleClicked(QGraphicsView *view, QGraphicsItem *item);

    void at_widget_destroyed();
    
private:
    /** Current mode. */
    Mode m_mode;

    /** Graphics scene. */
    QGraphicsScene *m_scene;

    /** Instrument manager for the scene. */
    InstrumentManager *m_manager;
    
    /** Central widget. */
    CentralWidget *m_centralWidget;

    /** Layout of the central widget. */
    CellLayout *m_centralLayout;

    /** Last bounds of central layout. */
    QRect m_centralLayoutBounds;

    /** All animated widgets. */
    QList<AnimatedWidget *> m_animatedWidgets;

    /** Bounding instrument. */
    BoundingInstrument *m_boundingInstrument;

    /** Widget that currently has focus. There can be only one such widget. */
    AnimatedWidget *m_focusedWidget;

    /** Whether the focused widget is expanded. */
    bool m_focusedExpanded;

    /** Whether the focused widget is zoomed. */
    bool m_focusedZoomed;
};

#endif // QN_SCENE_CONTROLLER_H
