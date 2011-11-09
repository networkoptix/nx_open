#ifndef unmoved_page_subitem_h_1828
#define unmoved_page_subitem_h_1828

#include "../unmoved_interactive_opacity_item.h"
class QPropertyAnimation;

class QnPageSubItem : public CLUnMovedInteractiveOpacityItem
{
	Q_OBJECT
    Q_PROPERTY(qreal scale  READ scale   WRITE setScale)
    Q_PROPERTY(QColor color  READ color   WRITE setColor)
signals:
    void onPageSelected(int page);

public:
    QnPageSubItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity, int number, bool current);
    ~QnPageSubItem();

    QRectF boundingRect () const;

    //void setScale(qreal factor);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    void mouseReleaseEvent ( QGraphicsSceneMouseEvent * e);
    void mousePressEvent ( QGraphicsSceneMouseEvent * e);


    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    QColor color() const;
    void setColor(QColor c);



    virtual void stopScaleAnimation();
    virtual void stopColorAnimation();

    void animateScale(qreal sc, unsigned int duration);
    void animateColor(QColor c, unsigned int duration);

private:
    unsigned int m_number;
    bool m_current;

    QPropertyAnimation* m_colorAnimation;
    QPropertyAnimation* m_scaleAnimation;

    QRectF m_boundingRect;

    QColor m_color;

    bool m_firstPaint;

};

#endif //unmoved_page_subitem_h_1828
