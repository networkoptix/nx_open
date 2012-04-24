#ifndef QN_CURTAIN_ITEM
#define QN_CURTAIN_ITEM

#include <QGraphicsObject>

/**
 * Item that fills the whole view with the given color. 
 */
class QnCurtainItem: public QGraphicsObject {
    Q_OBJECT;
    Q_PROPERTY(QColor color READ color WRITE setColor);

public:
    QnCurtainItem(QGraphicsItem *parent = NULL);

    virtual QRectF boundingRect() const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
    }

private:
    QColor m_color;
    QRectF m_boundingRect;
};


#endif // QN_CURTAIN_ITEM
