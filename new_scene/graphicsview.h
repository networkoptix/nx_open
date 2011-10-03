#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include <QtGui/QGraphicsView>

class AnimatedWidget;

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

Q_SIGNALS:

protected:
//    virtual void mousePressEvent(QMouseEvent *);
    virtual void mouseReleaseEvent(QMouseEvent *);
//    virtual void mouseDoubleClickEvent(QMouseEvent *);
//    virtual void mouseMoveEvent(QMouseEvent *);
#ifndef QT_NO_WHEELEVENT
    virtual void wheelEvent(QWheelEvent *);
#endif
    virtual void keyPressEvent(QKeyEvent *);
    virtual void keyReleaseEvent(QKeyEvent *);
/*    virtual void resizeEvent(QResizeEvent *);
#ifndef QT_NO_CONTEXTMENU
    virtual void contextMenuEvent(QContextMenuEvent *);
#endif
#ifndef QT_NO_DRAGANDDROP
    virtual void dragEnterEvent(QDragEnterEvent *);
    virtual void dragMoveEvent(QDragMoveEvent *);
    virtual void dragLeaveEvent(QDragLeaveEvent *);
    virtual void dropEvent(QDropEvent *);
#endif*/

    void drawBackground(QPainter *painter, const QRectF &rect);

private Q_SLOTS:
    void relayoutItemsActionTriggered();
    void widgetGeometryChanged();
    void itemClicked();
    void itemDoubleClicked();
    void itemDestroyed();
    void sceneSelectionChanged();

private:
    void createActions();

private:
    QGraphicsWidget *m_widget;
    QList<AnimatedWidget *> m_animatedWidgets;
    QGraphicsItemGroup *m_selectionGroup;
};

#endif // GRAPHICSVIEW_H
