// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tool_tip_widget.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QTextCursor>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsSceneWheelEvent>
#include <QtWidgets/QStyle>

#include <nx/utils/math/math.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <ui/common/palette.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>

using nx::vms::client::core::Geometry;

namespace  {

    static constexpr qreal kUseContentMargins = -1;

    void addEdgeTo(qreal x, qreal y, const QPointF &tailPos, qreal tailWidth, bool useTail, QPainterPath *path) {
        if(!useTail) {
            path->lineTo(x, y);
        } else {
            QPointF pos0 = path->currentPosition();
            QPointF pos1 = QPointF(x, y);
            QPointF delta = pos1 - pos0;

            qreal edgeLength = Geometry::length(delta);
            qreal segmentLength = qMin(tailWidth, edgeLength);
            qreal relativeSegmentLength = segmentLength / edgeLength;

            qreal t;
            Geometry::closestPoint(pos0, pos1, tailPos, &t);

            qreal t0 = t - relativeSegmentLength / 2.0;
            qreal t1 = t + relativeSegmentLength / 2.0;
            if(t0 < 0) {
                t1 += -t0;
                t0 = 0;
            } else if(t1 > 1.0) {
                t0 -= t1 - 1.0;
                t1 = 1.0;
            }

            path->lineTo(pos0 + t0 * delta);
            path->lineTo(tailPos);
            path->lineTo(pos0 + t1 * delta);
            path->lineTo(pos1);
        }
    }

    void addArcTo(qreal x, qreal y, QPainterPath *path) {
        QPointF pos0 = path->currentPosition();
        QPointF pos1 = QPointF(x, y);

        if(qFuzzyEquals(pos0, pos1))
            return;

        if(qFuzzyEquals(pos0.x(), pos1.x()) || qFuzzyEquals(pos0.y(), pos1.y())) {
            path->lineTo(x, y);
            return;
        }

        int angle;
        if(pos0.x() < pos1.x()) {
            if(pos0.y() < pos1.y()) {
                angle = 180;
            } else {
                angle = 270;
            }
        } else {
            if(pos0.y() < pos1.y()) {
                angle = 90;
            } else {
                angle = 0;
            }
        }

        QPointF center = (angle % 180 == 0) ? QPointF(pos1.x(), pos0.y()) : QPointF(pos0.x(), pos1.y());
        QPointF delta = (pos0 - center) + (pos1 - center);
        QRectF rect = QRectF(center - delta, center + delta).normalized();

        path->arcTo(rect, angle, 90);
    }

} // anonymous namespace

QnToolTipWidget::QnToolTipWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_shapeValid(false),
    m_tailWidth(5.0),
    m_roundingRadius(kUseContentMargins),
    m_autoSize(true)
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    m_textDocument.setDocumentMargin(0);
    m_textDocument.setIndentWidth(0);
}

QnToolTipWidget::~QnToolTipWidget()
{
}

QPointF QnToolTipWidget::tailPos() const {
    return m_tailPos;
}

Qt::Edges QnToolTipWidget::tailBorder() const
{
    QPointF distance = m_tailPos - Geometry::closestPoint(rect(), m_tailPos);
    if (qFuzzyIsNull(distance))
        return Qt::Edges();

    if (distance.y() < distance.x())
    {
        if (distance.y() < -distance.x())
            return Qt::TopEdge;

        return Qt::RightEdge;
    }
    else
    {
        if (distance.y() < -distance.x())
            return Qt::LeftEdge;

        return Qt::BottomEdge;
    }
}

void QnToolTipWidget::setTailPos(const QPointF &tailPos) {
    if(qFuzzyEquals(tailPos, m_tailPos))
        return;

    m_tailPos = tailPos;

    /* Note that changing tailPos position affects bounding rect,
     * but not the actual widget's geometry. */
    prepareGeometryChange();
    invalidateShape();

    emit tailPosChanged();
}

qreal QnToolTipWidget::tailWidth() const {
    return m_tailWidth;
}

void QnToolTipWidget::setTailWidth(qreal tailWidth) {
    if (qFuzzyEquals(tailWidth, m_tailWidth))
        return;

    m_tailWidth = tailWidth;

    invalidateShape();
}

void QnToolTipWidget::pointTo(const QPointF& pos)
{
    QPointF parentTailPos = mapToParent(m_tailPos);
    QPointF parentZeroPos = mapToParent(QPointF(0, 0));
    QPointF newPos = pos + parentZeroPos - parentTailPos;
    if ((this->pos() - newPos).manhattanLength() < 1.0)
        return;

    setPos(newPos);
}

QString QnToolTipWidget::text() const
{
    return m_textDocument.toPlainText();
}

void QnToolTipWidget::setText(const QString& text, qreal lineHeight /* = 100 */)
{
    if (text != m_text || lineHeight != m_lineHeight)
    {
        m_text = text;
        m_lineHeight = lineHeight;
        generateTextPixmap();
        updateHeight();
    }
}

void QnToolTipWidget::setTextWidth(qreal width)
{
    if (!qFuzzyCompare(width, m_textWidth))
    {
        m_textWidth = width;
        m_textDocument.setTextWidth(width);

        const auto& margins = contentsMargins();
        auto newGeometry = geometry();
        auto widgetWidth = margins.left() + margins.right() + width;
        newGeometry.setWidth(widgetWidth);
        setPreferredWidth(widgetWidth);
        setMinimumWidth(widgetWidth);
        setGeometry(newGeometry);

        generateTextPixmap();
        updateHeight();
    }
}

void QnToolTipWidget::setTextColor(const QColor& color)
{
    if (color != m_textColor)
    {
        m_textColor = color;
        generateTextPixmap();
        updateHeight();
    }
}

QRectF QnToolTipWidget::boundingRect() const
{
    ensureShape();
    return m_boundingRect;
}

void QnToolTipWidget::setRoundingRadius(qreal radius)
{
    if (qFuzzyEquals(m_roundingRadius, radius))
        return;

    m_roundingRadius = radius;
    ensureShape();
}

void QnToolTipWidget::setGeometry(const QRectF &rect) {
    QSizeF oldSize = size();

    base_type::setGeometry(rect);

    if(!qFuzzyEquals(size(), oldSize))
        invalidateShape();
}

void QnToolTipWidget::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    ensureShape();

    /* Render background. */
    paintSharp(painter,
        [this, option, widget](QPainter* painter)
        {
            QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
            QnScopedPainterPenRollback penRollback(painter, QPen(frameBrush(), frameWidth()));
            QnScopedPainterBrushRollback brushRollback(painter, windowBrush());
            painter->drawPath(m_borderShape);

            const auto& margins = contentsMargins();
            painter->drawPixmap(margins.left(), margins.top(), m_textPixmap);
        });
}

void QnToolTipWidget::wheelEvent(QGraphicsSceneWheelEvent *event) {
    event->ignore();

    base_type::wheelEvent(event);
}

void QnToolTipWidget::invalidateShape() {
    m_shapeValid = false;
}

void QnToolTipWidget::ensureShape() const {
    if(m_shapeValid)
        return;

    qreal l = m_roundingRadius;
    qreal t = m_roundingRadius;
    qreal r = m_roundingRadius;
    qreal b = m_roundingRadius;

    if (m_roundingRadius == kUseContentMargins)
        getContentsMargins(&l, &t, &r, &b);

    Qt::Edges tailBorder = this->tailBorder();
    QRectF rect = this->rect();

    /* CCW traversal. */
    QPainterPath borderShape;
    borderShape.moveTo(rect.left(), rect.top() + t);
    addEdgeTo(rect.left(),          rect.bottom() - b,  m_tailPos, m_tailWidth, tailBorder.testFlag(Qt::LeftEdge), &borderShape);
     addArcTo(rect.left() + l,      rect.bottom(),      &borderShape);
    addEdgeTo(rect.right() - r,     rect.bottom(),      m_tailPos, m_tailWidth, tailBorder.testFlag(Qt::BottomEdge), &borderShape);
     addArcTo(rect.right(),         rect.bottom() - b,  &borderShape);
    addEdgeTo(rect.right(),         rect.top() + t,     m_tailPos, m_tailWidth, tailBorder.testFlag(Qt::RightEdge), &borderShape);
     addArcTo(rect.right() - r,     rect.top(),         &borderShape);
    addEdgeTo(rect.left() + l,      rect.top(),         m_tailPos, m_tailWidth, tailBorder.testFlag(Qt::TopEdge), &borderShape);
     addArcTo(rect.left(),          rect.top() + t,     &borderShape);
    borderShape.closeSubpath();

    m_borderShape = borderShape;
    m_boundingRect = borderShape.boundingRect(); // TODO: add m_borderPen.widthF() / 2
    m_shapeValid = true;
}

void QnToolTipWidget::updateHeight()
{
    auto margins = contentsMargins();
    auto textHeight = m_textDocument.size().height();
    auto newHeight = margins.top() + margins.bottom() + textHeight;
    if (newHeight != m_oldHeight)
    {
        m_oldHeight = newHeight;
        auto newGeometry = geometry();
        newGeometry.setHeight(newHeight);
        setGeometry(newGeometry);
        update();
    }
}

void QnToolTipWidget::generateTextPixmap()
{
    m_textDocument.setDefaultStyleSheet(
        QStringLiteral("body { color: %1; background-color: transparent; }")
            .arg(m_textColor.name()));
    m_textDocument.setHtml(QStringLiteral("<body>%1</body>").arg(m_text));

    QTextCursor cursor(&m_textDocument);
    cursor.select(QTextCursor::Document);

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(m_lineHeight, QTextBlockFormat::ProportionalHeight);
    cursor.setBlockFormat(blockFormat);

    QTextOption option = m_textDocument.defaultTextOption();
    option.setAlignment(Qt::AlignCenter);
    m_textDocument.setDefaultTextOption(option);

    auto ratio = qApp->devicePixelRatio();
    const auto& size = m_textDocument.size() * ratio;

    if (size.isValid())
    {
        m_textPixmap = QPixmap(size.toSize());
        m_textPixmap.setDevicePixelRatio(ratio);
        m_textPixmap.fill(Qt::transparent);

        QPainter painter(&m_textPixmap);
        paintSharp(&painter,
            [this](QPainter* painter)
            {
                QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
                m_textDocument.drawContents(painter);
            });
    }
}
