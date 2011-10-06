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

    bool isGridVisible() const;
    void setGridVisible(bool visible, int timeout = 500);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

protected:
    float gridOpacity() const;
    void setGridOpacity(float opacity);

private:
    bool m_gridVisible;
    float m_gridOpacity;
    QPropertyAnimation *m_gridOpacityAnimation;
};

#endif // CENTRALDWIDGET_H
