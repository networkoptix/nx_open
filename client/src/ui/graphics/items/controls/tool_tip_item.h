#ifndef QN_TOOL_TIP_ITEM_H
#define QN_TOOL_TIP_ITEM_H

#include <QtGui/QGraphicsItem>
#include <QtGui/QPen>
#include <QtGui/QBrush>
#include <QtGui/QFont>

class QnToolTipItem: public QGraphicsObject {
    Q_OBJECT;

    typedef QGraphicsObject base_type;

public:
    QnToolTipItem(QGraphicsItem *parent = 0);
    virtual ~QnToolTipItem();

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


#endif // QN_TOOL_TIP_ITEM_H
