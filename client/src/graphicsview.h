#ifndef GRAPHICSVIEW_H
#define GRAPHICSVIEW_H

#include <QtGui/QGraphicsView>

class QGraphicsWidget;

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

protected:
    virtual void mouseReleaseEvent(QMouseEvent *);
    virtual void keyPressEvent(QKeyEvent *);
    virtual void keyReleaseEvent(QKeyEvent *);

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
    QList<AnimatedWidget *> m_animatedWidgets;
};

#endif // GRAPHICSVIEW_H
