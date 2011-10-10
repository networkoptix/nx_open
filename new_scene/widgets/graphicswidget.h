#ifndef NOTIFYINGWIDGET_H
#define NOTIFYINGWIDGET_H

#include <QGraphicsWidget>
#include <QScopedPointer>

/* A quick workaround. Remove this once we have a better solution. */
#undef override
#define override

class GraphicsWidgetPrivate;

class GraphicsWidget: public QGraphicsWidget
{
    Q_OBJECT;

    typedef QGraphicsWidget base_type;

public:
    GraphicsWidget(QGraphicsItem *parent = NULL);

    virtual ~GraphicsWidget();

signals:
    void clicked();
    void doubleClicked();
    void resizingStarted();
    void resizingFinished();
    void draggingStarted();
    void draggingFinished();

protected:
    GraphicsWidget(QGraphicsItem *parent, GraphicsWidgetPrivate *dd);

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;

protected:
    QScopedPointer<GraphicsWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(GraphicsWidget);
};

#endif // NOTIFYINGWIDGET_H
