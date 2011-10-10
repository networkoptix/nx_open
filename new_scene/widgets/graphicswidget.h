#ifndef NOTIFYINGWIDGET_H
#define NOTIFYINGWIDGET_H

#include <QGraphicsWidget>
#include <QScopedPointer>

/* A quick workaround. Remove this once we have a better solution. */
#undef override
#define override

class NotifyingWidgetPrivate;

class NotifyingWidget: public QGraphicsWidget
{
    Q_OBJECT;

    typedef QGraphicsWidget base_type;

public:
    NotifyingWidget(QGraphicsItem *parent = NULL);

    virtual ~NotifyingWidget();

signals:
    void clicked();
    void doubleClicked();
    void resizingStarted();
    void resizingFinished();
    void draggingStarted();
    void draggingFinished();

protected:
    NotifyingWidget(QGraphicsItem *parent, NotifyingWidgetPrivate *dd);

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    virtual bool windowFrameEvent(QEvent *event) override;

protected:
    QScopedPointer<NotifyingWidgetPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(NotifyingWidget);
};

#endif // NOTIFYINGWIDGET_H
