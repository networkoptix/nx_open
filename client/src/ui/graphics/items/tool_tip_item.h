#ifndef QN_TOOL_TIP_ITEM_H
#define QN_TOOL_TIP_ITEM_H

#include <QtGui/QGraphicsItem>
#include <QtGui/QPen>
#include <QtGui/QBrush>
#include <QtGui/QFont>

class QnToolTipItem: public QGraphicsItem {
public:
    QnToolTipItem(QGraphicsItem *parent = 0);

    const QString &text() const;
    void setText(const QString &text);

    const QFont &font() const;
    void setFont(const QFont &font);

    const QPen &textPen() const;
    void setTextPen(const QPen &textPen);

    const QPen &borderPen() const;
    void setBorderPen(const QPen &borderPen);

    const QBrush &brush() const;
    void setBrush(const QBrush &brush);

    virtual QRectF boundingRect() const override;
    virtual QPainterPath shape() const override;

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;

private:
    void invalidateShape();
    void ensureShape() const;
    void updateTextSize();

private:
    mutable bool m_shapeValid;
    mutable QPainterPath m_itemShape, m_borderShape;
    mutable QRectF m_boundingRect;

    QPen m_borderPen;
    QPen m_textPen;
    QBrush m_brush;
    QString m_text;
    QSize m_textSize;
    QFont m_font;
};


class QnStyledToolTipItem: public QnToolTipItem {
public:
    QnStyledToolTipItem(QGraphicsItem *parent = 0) : QnToolTipItem(parent)
    {
        setFont(QFont()); /* Default application font. */
        setTextPen(QColor(63, 159, 216));
        setBrush(QColor(0, 0, 0, 255));
        setBorderPen(QPen(QColor(203, 210, 233, 128), 0.7));
    }
};

#endif // TOOLTIPITEM_H
