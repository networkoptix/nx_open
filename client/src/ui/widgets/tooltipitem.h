#ifndef TOOLTIPITEM_H
#define TOOLTIPITEM_H

#include <QtGui/QGraphicsPixmapItem>

class ToolTipItem: public QGraphicsItem
{
public:
    ToolTipItem(QGraphicsItem *parent = 0);

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

    virtual void updateParameters(QPainterPath *borderShape, QRectF *contentRect) const;

private:
    void ensureParameters() const;
    void invalidateParameters();
    void updateTextSize();

private:
    mutable QPainterPath m_itemShape, m_borderShape;
    mutable QRectF m_boundingRect, m_contentRect;
    mutable bool m_parametersValid;

    QPen m_borderPen;
    QPen m_textPen;
    QBrush m_brush;
    
    QString m_text;
    QSize m_textSize;
    QFont m_font;
};


class StyledToolTipItem: public ToolTipItem
{
public:
    StyledToolTipItem(QGraphicsItem *parent = 0) : ToolTipItem(parent)
    {
        setFont(QFont()); /* Default application font. */
        setTextPen(QColor(63, 159, 216));
        setBrush(QColor(0, 0, 0, 255));
        setBorderPen(QPen(QColor(203, 210, 233, 128), 0.7));
    }
};

#endif // TOOLTIPITEM_H
