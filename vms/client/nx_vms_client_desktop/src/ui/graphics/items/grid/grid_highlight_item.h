#ifndef QN_GRID_HIGHLIGHT_ITEM_H
#define QN_GRID_HIGHLIGHT_ITEM_H

#include <QtWidgets/QGraphicsObject>

class QnGridHighlightItem: public QGraphicsObject {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)

public:
    QnGridHighlightItem(QGraphicsItem *parent = NULL);

    virtual ~QnGridHighlightItem();

    virtual QRectF boundingRect() const override;

    const QColor &color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
    }

    const QRectF &rect() const {
        return m_rect;
    }

    void setRect(const QRectF &rect);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QRectF m_rect;
    QColor m_color;
};

#endif // QN_GRID_HIGHLIGHT_ITEM_H
