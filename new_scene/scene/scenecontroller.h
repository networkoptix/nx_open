#ifndef QN_SCENE_CONTROLLER_H
#define QN_SCENE_CONTROLLER_H

#include <QObject>
#include <QScopedPointer>
#include <instruments/instrumentutility.h>

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;

class SceneControllerPrivate;

class SceneController: public QObject, protected InstrumentUtility {
    Q_OBJECT;
public:
    enum Mode {
        VIEWING,
        EDITING
    };

    SceneController(QGraphicsView *view, QObject *parent = NULL);

    virtual ~SceneController();

    QGraphicsScene *scene() const;

    QGraphicsView *view() const;

    Mode mode() const;

    void setMode(Mode mode);

protected:
    void relayoutItems(int rowCount, int columnCount, const QByteArray &preset);

private slots:
    void at_relayoutAction_triggered();
    void at_changeModeAction_triggered();

    void at_centralWidget_geometryChanged();

    void at_widget_resizingStarted();
    void at_widget_resizingFinished();

    void at_draggingStarted(QGraphicsView *view, QList<QGraphicsItem *> items);
    void at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items);

    void at_item_clicked(QGraphicsView *view, QGraphicsItem *item);
    void at_item_doubleClicked(QGraphicsView *view, QGraphicsItem *item);

    void at_scene_clicked(QGraphicsView *view);
    void at_scene_doubleClicked(QGraphicsView *view);

    void at_widget_destroyed();

    void at_zoomAnimation_finished();
    
private:
    QScopedPointer<SceneControllerPrivate> d_ptr;

    Q_DECLARE_PRIVATE(SceneController);
};

#endif // QN_SCENE_CONTROLLER_H
