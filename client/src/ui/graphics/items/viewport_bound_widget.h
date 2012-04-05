#ifndef QN_VIEWPORT_BOUND_WIDGET_H
#define QN_VIEWPORT_BOUND_WIDGET_H

#include <QGraphicsWidget>

class QnViewportBoundWidget: public QGraphicsWidget {
    Q_OBJECT;

    typedef QGraphicsWidget base_type;

public:
    QnViewportBoundWidget(QGraphicsItem *parent = NULL);
    virtual ~QnViewportBoundWidget();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QTransform m_sceneToViewport;
    QWeakPointer<QGraphicsView> m_lastView;
};


#endif // QN_VIEWPORT_BOUND_WIDGET_H
