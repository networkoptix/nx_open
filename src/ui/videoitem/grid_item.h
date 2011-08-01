#ifndef CLGridItem_h_1841
#define CLGridItem_h_1841

#include <QtGui/QGraphicsObject>

#include "abstract_scene_item.h"

class GraphicsView;

class CLGridItem : public QGraphicsObject
{
    Q_OBJECT

public:
    CLGridItem(GraphicsView *view);
    ~CLGridItem();

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);
    QRectF boundingRect() const;

    // animates visibility changing
    void setVisibleAnimated(bool visible, int time_ms = 1000);
    inline void showAnimated(int time_ms = 1000) { setVisibleAnimated(true, time_ms); }
    inline void hideAnimated(int time_ms = 1000) { setVisibleAnimated(false, time_ms); }

private Q_SLOTS:
    void stopAnimation();

private:
    GraphicsView *const m_view;
    QPropertyAnimation *m_animation;
};

#endif // CLGridItem
