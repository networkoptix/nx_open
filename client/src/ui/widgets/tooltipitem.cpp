#include "tooltipitem.h"

#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneEvent>

ToolTipItem::ToolTipItem(QGraphicsItem *parent):
    QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);

    recalcShape();
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
    recalcTextSize();
    update();
}

const QFont &ToolTipItem::font() const
{
    return m_font;
}

void ToolTipItem::setFont(const QFont &font)
{
    m_font = font;
    recalcTextSize();
    update();
}

const QPen &ToolTipItem::textPen() const
{
    return m_textPen;
}

void ToolTipItem::setTextPen(const QPen &textPen)
{
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
    recalcShape();
    update();
}

const QBrush &ToolTipItem::brush() const
{
    return m_brush;
}

void ToolTipItem::setBrush(const QBrush &brush)
{
    m_brush = brush;
    update();
}

QRectF ToolTipItem::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath ToolTipItem::shape() const
{
    return m_itemShape;
}

void ToolTipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    /* Draw background. */
    painter->fillPath(m_borderShape, m_brush);

    /* Draw text. */
    //painter->setFont(m_font);
    //painter->setPen(m_textPen);
    //painter->drawStaticText(m_textRect.topLeft(), m_staticText);
    painter->drawPixmap(m_textRect.topLeft(), m_textPixmap);

    /* Draw outline. */
    painter->setPen(m_borderPen);
    painter->drawPath(m_borderShape);
}

void ToolTipItem::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    event->ignore();

    QGraphicsItem::wheelEvent(event);
}

void ToolTipItem::recalcTextSize()
{
    recalcShape();
}

void ToolTipItem::recalcShape()
{
    {
        static const qreal roundingRadius = 5.0;
        static const qreal padding = -1.0;
        static const qreal arrowHeight = 8.0;
        static const qreal arrowWidth = 8.0;

        QFontMetrics m(m_font);
        const QSizeF textSize = m.size(Qt::TextSingleLine, m_text);
        const QSizeF paddingSize = textSize + QSizeF(padding * 2, padding * 2);

        m_textPixmap = QPixmap(textSize.toSize());
        m_textPixmap.fill(QColor(0,0,0,0));
        QPainter textPainter(&m_textPixmap); // TODO: "QPainter::begin: Paint device returned engine == 0, type: 2"
        textPainter.setFont(m_font);
        textPainter.setPen(m_textPen);
        textPainter.drawText(0, m.ascent(), m_text);

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
        m_borderShape.lineTo(-hw - roundingRadius, -arrowHeight);
        m_borderShape.arcTo(QRectF(-hw - roundingRadius, -arrowHeight - roundingRadius, roundingRadius, roundingRadius), 180, 90);

        m_borderShape.lineTo(-arrowWidth / 2, -arrowHeight);
        m_borderShape.closeSubpath();

        m_textRect = QRectF(-textSize.width() / 2, -arrowHeight - roundingRadius - padding - textSize.height(), textSize.width(), textSize.height());
    }

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
