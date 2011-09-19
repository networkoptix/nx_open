#ifndef TOOLTIPITEM_H
#define TOOLTIPITEM_H

#include <QtGui/QGraphicsPixmapItem>

class ToolTipItem: public QGraphicsPixmapItem
{
public:
    ToolTipItem(QGraphicsItem *parent = 0);
    ToolTipItem(const QPixmap &pixmap, QGraphicsItem *parent = 0);

    QString text() const;
    void setText(const QString &text);

    QFont textFont() const;
    void setTextFont(const QFont &font);

    QColor textColor() const;
    void setTextColor(const QColor &color);

    QRectF textRect() const;
    void setTextRect(const QRectF &rect);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *event);

private:
    QString m_text;
    QFont m_textFont;
    QColor m_textColor;
    QRectF m_textRect;
};


#include "ui/skin.h"

class StyledToolTipItem: public ToolTipItem
{
public:
    StyledToolTipItem(QGraphicsItem *parent = 0) : ToolTipItem(parent)
    {
        setPixmap(Skin::path(QLatin1String("time-window.png")));
        setTransformationMode(Qt::FastTransformation);
        setShapeMode(QGraphicsPixmapItem::HeuristicMaskShape);
        setOffset(-pixmap().width() / 2, 0);
        setTextFont(QFont()); // default application font
        setTextColor(QColor(63, 159, 216));
        setTextRect(ToolTipItem::boundingRect().adjusted(5, 5, -5, -13));
    }
};

#endif // TOOLTIPITEM_H
