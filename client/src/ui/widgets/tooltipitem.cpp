#include "tooltipitem.h"
#include <cmath>
#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneEvent>
#include <utils/common/scoped_painter_rollback.h>

namespace 
{
    const qreal roundingRadius = 5.0;
    const qreal padding = -1.0;
    const qreal arrowHeight = 8.0;
    const qreal arrowWidth = 8.0;
}

ToolTipItem::ToolTipItem(QGraphicsItem *parent):
    QGraphicsItem(parent),
    m_shapeValid(false)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);

    setCacheMode(QGraphicsItem::ItemCoordinateCache);
}

const QString &ToolTipItem::text() const
{
    return m_text;
}

void ToolTipItem::setText(const QString &text)
{
    if (m_text == text)
        return;

    m_text = text;
    updateTextSize();
    update();
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
    update();
}

const QPen &ToolTipItem::textPen() const
{
    return m_textPen;
}

void ToolTipItem::setTextPen(const QPen &textPen)
{
    if(m_textPen == textPen)
        return;

    m_textPen = textPen;
    update();
}

const QPen &ToolTipItem::borderPen() const
{
    return m_borderPen;
}

void ToolTipItem::setBorderPen(const QPen &borderPen)
{
    if (m_borderPen == borderPen)
        return;

    m_borderPen = borderPen;
    invalidateShape();
    update();
}

const QBrush &ToolTipItem::brush() const
{
    return m_brush;
}

void ToolTipItem::setBrush(const QBrush &brush)
{
    if(m_brush == brush)
        return;

    m_brush = brush;
    invalidateShape();
    update();
}

QRectF ToolTipItem::boundingRect() const
{
    ensureShape();

    return m_boundingRect;
}

QPainterPath ToolTipItem::shape() const
{
    ensureShape();

    return m_itemShape;
}

void ToolTipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    /* Render background. */
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    painter->setPen(m_borderPen);
    painter->setBrush(m_brush);
    painter->drawPath(m_borderShape);

    /* Render text. */
    painter->setFont(m_font);
    painter->setPen(m_textPen);
    painter->drawText(QRectF(-m_textSize.width() / 2, -arrowHeight - roundingRadius - padding - m_textSize.height(), m_textSize.width(), m_textSize.height()), Qt::AlignCenter, m_text);
}

void ToolTipItem::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    event->ignore();

    QGraphicsItem::wheelEvent(event);
}

void ToolTipItem::invalidateShape() 
{
    m_shapeValid = false;

    prepareGeometryChange();
}

void ToolTipItem::ensureShape() const 
{
    if(m_shapeValid)
        return;

    /* Prepare border shape. */
    {
        const QSizeF paddingSize = m_textSize + QSizeF(padding * 2, padding * 2);

        /* Half padding width */
        const float hw = paddingSize.width() / 2;

        m_borderShape = QPainterPath();
        m_borderShape.moveTo(0.0, 0.0);
        m_borderShape.lineTo(arrowWidth / 2, -arrowHeight);

        m_borderShape.lineTo(hw, -arrowHeight);
        m_borderShape.arcTo(QRectF(hw, -arrowHeight - roundingRadius, roundingRadius, roundingRadius), 270, 90);
        m_borderShape.lineTo(hw + roundingRadius, -arrowHeight - paddingSize.height() - roundingRadius);
        m_borderShape.arcTo(QRectF(hw, -arrowHeight - paddingSize.height() - 2 * roundingRadius, roundingRadius, roundingRadius), 0, 90);
        m_borderShape.lineTo(-hw, -arrowHeight - paddingSize.height() - 2 * roundingRadius);
        m_borderShape.arcTo(QRectF(-hw - roundingRadius, -arrowHeight - paddingSize.height() - 2 * roundingRadius, roundingRadius, roundingRadius), 90, 90);
        m_borderShape.lineTo(-hw - roundingRadius, -arrowHeight - roundingRadius);
        m_borderShape.arcTo(QRectF(-hw - roundingRadius, -arrowHeight - roundingRadius, roundingRadius, roundingRadius), 180, 90);

        m_borderShape.lineTo(-arrowWidth / 2, -arrowHeight);
        m_borderShape.closeSubpath();
    }

    /* Calculate item shape & bounding rect. */
    if (m_borderPen.isCosmetic()) {
        m_itemShape = m_borderShape;
    } else {
        QPainterPathStroker stroker;
        stroker.setWidth(m_borderPen.widthF());
        m_itemShape = stroker.createStroke(m_borderShape);
        m_itemShape.addPath(m_borderShape);
        m_itemShape = m_itemShape.simplified();
    }
    m_boundingRect = m_itemShape.boundingRect();
}

void ToolTipItem::updateTextSize() 
{
    QFontMetrics metrics(m_font);
    QSize textSize = metrics.size(0, m_text);
    if(textSize == m_textSize)
        return;

    m_textSize = textSize;
    invalidateShape();
}
