#ifndef CENTRALDWIDGET_H
#define CENTRALDWIDGET_H

#include <QtGui/QGraphicsWidget>

class QPropertyAnimation;

class CentralWidget : public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(float gridOpacity READ gridOpacity WRITE setGridOpacity DESIGNABLE false FINAL)

public:
    CentralWidget(QGraphicsItem *parent = 0);
    ~CentralWidget();

    float gridOpacity() const;
    void setGridOpacity(float opacity);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
    float m_gridOpacity;
    QPropertyAnimation *m_gridOpacityAnimation;
};

#endif // CENTRALDWIDGET_H
