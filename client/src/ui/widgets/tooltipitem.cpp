#include "tooltipitem.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneWheelEvent>

ToolTipItem::ToolTipItem(QGraphicsItem *parent):
    QGraphicsItem(parent),
    m_parametersValid(false)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
}

const QString &ToolTipItem::text() const
{
    return m_text;
}

void ToolTipItem::setText(const QString &text)
{
    if(m_text == text)
        return;

    m_text = text;
    updateTextSize();
}

const QFont &ToolTipItem::font() const
{
    return m_font;
}

void ToolTipItem::setFont(const QFont &font)
{
    if(m_font == font)
        return;

    m_font = font;
    updateTextSize();
}

const QPen &ToolTipItem::textPen() const
{
    return m_textPen;
}

void ToolTipItem::setTextPen(const QPen &textPen)
{
    m_textPen = textPen;
}

const QPen &ToolTipItem::borderPen() const
{
    return m_borderPen;
}

void ToolTipItem::setBorderPen(const QPen &borderPen)
{
    if(m_borderPen == borderPen)
        return;

    m_borderPen = borderPen;
    invalidateParameters();
}

const QBrush &ToolTipItem::brush() const
{
    return m_brush;
}

void ToolTipItem::setBrush(const QBrush &brush)
{
    m_brush = brush;
}

QRectF ToolTipItem::boundingRect() const
{
    ensureParameters();

    return m_boundingRect;
}

QPainterPath ToolTipItem::shape() const
{
    ensureParameters();

    return m_itemShape;
}

void ToolTipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    ensureParameters();

    /* Draw background. */
    painter->fillPath(m_borderShape, m_brush);

    /* Draw text. */
    painter->setFont(m_font);
    painter->setPen(m_textPen);
    painter->drawText(m_contentRect, Qt::AlignCenter | Qt::TextDontClip, m_text);

    /* Draw outline. */
    painter->setPen(m_borderPen);
    painter->drawPath(m_borderShape);
}

void ToolTipItem::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    event->ignore();

    QGraphicsItem::wheelEvent(event);
}

void ToolTipItem::updateTextSize()
{
    QSize textSize = QFontMetrics(font()).size(Qt::AlignCenter, text());
    if(textSize == m_textSize)
        return;

    m_textSize = textSize;
    invalidateParameters();
}

namespace
{
    static const qreal roundingRadius = 5.0;
    static const qreal padding = -2.0;
    static const qreal arrowHeight = 10.0;
    static const qreal arrowWidth = 8.0;
}

void ToolTipItem::updateParameters(QPainterPath *borderShape, QRectF *contentRect) const
{
    QSizeF contentSize = m_textSize;
    QSizeF paddingSize = contentSize + QSizeF(padding * 2, padding * 2);

    /* Half padding width */
    qreal hw = paddingSize.width() / 2;

    *borderShape = QPainterPath();
    borderShape->moveTo(0.0, 0.0);
    borderShape->lineTo(arrowWidth / 2, -arrowHeight);

    borderShape->lineTo( hw,                  -arrowHeight);
    borderShape->arcTo(QRectF( hw,                  -arrowHeight                            - roundingRadius, roundingRadius, roundingRadius), 270, 90);
    borderShape->lineTo( hw + roundingRadius, -arrowHeight - paddingSize.height() -     roundingRadius);
    borderShape->arcTo(QRectF( hw,                  -arrowHeight - paddingSize.height() - 2 * roundingRadius, roundingRadius, roundingRadius), 0, 90);
    borderShape->lineTo(-hw,                  -arrowHeight - paddingSize.height() - 2 * roundingRadius);
    borderShape->arcTo(QRectF(-hw - roundingRadius, -arrowHeight - paddingSize.height() - 2 * roundingRadius, roundingRadius, roundingRadius), 90, 90);
    borderShape->lineTo(-hw - roundingRadius, -arrowHeight);
    borderShape->arcTo(QRectF(-hw - roundingRadius, -arrowHeight                            - roundingRadius, roundingRadius, roundingRadius), 180, 90);

    borderShape->lineTo(-arrowWidth / 2, -arrowHeight);
    borderShape->closeSubpath();

    *contentRect = QRectF(-contentSize.width() / 2, -arrowHeight - roundingRadius - padding - contentSize.height(), contentSize.width(), contentSize.height());
}

void ToolTipItem::ensureParameters() const
{
    if(m_parametersValid)
        return;

    updateParameters(&m_borderShape, &m_contentRect);

    if(m_borderPen.isCosmetic()) {
        m_itemShape = m_borderShape;
    } else {
        QPainterPathStroker stroker;
        stroker.setWidth(m_borderPen.widthF());
        m_itemShape = stroker.createStroke(m_borderShape);
        m_itemShape.addPath(m_borderShape);
        m_itemShape = m_itemShape.simplified();
    }

    m_boundingRect = m_itemShape.boundingRect();

    m_parametersValid = true;
}

void ToolTipItem::invalidateParameters()
{
    m_parametersValid = false;
}
