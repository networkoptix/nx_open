#include "tooltipitem.h"

ToolTipItem::ToolTipItem(QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{
}

ToolTipItem::ToolTipItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
{
}

QString ToolTipItem::text() const
{
    return m_text;
}

void ToolTipItem::setText(const QString &text)
{
    m_text = text;
    update();
}

QFont ToolTipItem::textFont() const
{
    return m_textFont;
}

void ToolTipItem::setTextFont(const QFont &font)
{
    m_textFont = font;
    update();
}

QColor ToolTipItem::textColor() const
{
    return m_textColor;
}

void ToolTipItem::setTextColor(const QColor &color)
{
    m_textColor = color;
    update();
}

QRectF ToolTipItem::textRect() const
{
    return m_textRect;
}

void ToolTipItem::setTextRect(const QRectF &rect)
{
    m_textRect = rect;
    update();
}

void ToolTipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QGraphicsPixmapItem::paint(painter, option, widget);

    painter->setFont(m_textFont);
    painter->setPen(m_textColor);
    painter->drawText(m_textRect.isValid() ? m_textRect : boundingRect(), Qt::AlignCenter, m_text);
}

void ToolTipItem::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    event->ignore();

    QGraphicsItem::wheelEvent(event);
}
