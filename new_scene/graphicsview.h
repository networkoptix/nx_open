#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include <QtGui/QGraphicsView>

class GraphicsView: public QGraphicsView
{
    Q_OBJECT

public:
    GraphicsView(QWidget *parent = 0);
    ~GraphicsView();

    bool isEditMode() const;
    void setEditMode(bool interactive);

    void relayoutItems(int rowCount, int columnCount, const QByteArray &preset = QByteArray());
    void invalidateLayout();

    // helpers
    inline QRectF mapRectToScene(const QRect &rect) const
    { return QRectF(mapToScene(rect.topLeft()), mapToScene(rect.bottomRight())); }
    inline QRect mapRectFromScene(const QRectF &rect) const
    { return QRect(mapFromScene(rect.topLeft()), mapFromScene(rect.bottomRight())); }

protected:
    virtual void keyPressEvent(QKeyEvent *);
    virtual void keyReleaseEvent(QKeyEvent *);

    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private Q_SLOTS:
    void relayoutItemsActionTriggered();
    void widgetGeometryChanged();
    void itemClicked();
    void itemDoubleClicked();
    void itemDestroyed();

private:
    void createActions();

private:
    QGraphicsWidget *m_widget;
    QList<QGraphicsWidget *> m_animatedWidgets;

    enum DragState {
        INITIAL,
        PREPAIRING,
        DRAGGING
    };

    DragState mDragState;
    QPoint mMousePressPos;
    QPointF mLastMouseScenePos; 
};

#endif // GRAPHICSVIEW_H
