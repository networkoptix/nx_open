#include "tool_tip_widget.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QApplication>
#include <QtGui/QStyle>

#include <utils/math/math.h>
#include <utils/common/scoped_painter_rollback.h>

#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/common/geometry.h>

namespace  {
    /*const qreal roundingRadius = 5.0;
    const qreal padding = -1.0;
    const qreal arrowHeight = 8.0;
    const qreal arrowWidth = 8.0;*/

    void addEdgeTo(qreal x, qreal y, const QPointF &tailPos, qreal tailWidth, bool useTail, QPainterPath *path) {
        if(!useTail) {
            path->lineTo(x, y);
        } else {
            QPointF pos0 = path->currentPosition();
            QPointF pos1 = QPointF(x, y);
            QPointF delta = pos1 - pos0;

            qreal edgeLength = QnGeometry::length(delta);
            qreal segmentLength = qMin(tailWidth, edgeLength);
            qreal relativeSegmentLength = segmentLength / edgeLength;

            qreal t;
            QnGeometry::closestPoint(pos0, pos1, tailPos, &t);

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
        }
    }

    void addArcTo(qreal x, qreal y, QPainterPath *path) {
        QPointF pos0 = path->currentPosition();
        QPointF pos1 = QPointF(x, y);

        if(qFuzzyCompare(pos0, pos1))
            return;

        if(qFuzzyCompare(pos0.x(), pos1.x()) || qFuzzyCompare(pos0.y(), pos1.y())) {
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
    m_tailWidth(5.0)
{
    //setFlag(ItemIsMovable, false);
    //setFlag(ItemIsSelectable, false);

    //setCacheMode(ItemCoordinateCache);
    //setProperty(Qn::NoHandScrollOver, true);

    /* Set up default colors. */
    /*QStyle *style = QApplication::style();
    setTextPen(QPen(style->standardPalette().windowText(), 0));
    setBorderPen(QPen(style->standardPalette().windowText(), 0));
    setBrush(style->standardPalette().window());*/

    /*QFont fixedFont = QApplication::font();
    fixedFont.setPixelSize(14);
    setFont(fixedFont);*/

    /* Update. */
    //updateTextSize();
}

QnToolTipWidget::~QnToolTipWidget() {
    return;
}

QPointF QnToolTipWidget::tailPos() const {
    return m_tailPos;
}

Qn::Border QnToolTipWidget::tailBorder() const {
    QPointF distance = m_tailPos - QnGeometry::closestPoint(rect(), m_tailPos);
    if(qFuzzyIsNull(distance))
        return Qn::NoBorders;
    
    if(distance.y() < distance.x()) {
        if(distance.y() < -distance.x()) {
            return Qn::TopBorder;
        } else {
            return Qn::RightBorder;
        }
    } else {
        if(distance.y() < -distance.x()) {
            return Qn::LeftBorder;
        } else {
            return Qn::BottomBorder;
        }
    }
}

void QnToolTipWidget::setTailPos(const QPointF &tailPos) {
    if(qFuzzyCompare(tailPos, m_tailPos))
        return;

    m_tailPos = tailPos;

    /* Note that changing tailPos position affects bounding rect, 
     * but not the actual widget's geometry. */
    prepareGeometryChange();
    invalidateShape();
}

qreal QnToolTipWidget::tailWidth() const {
    return m_tailWidth;
}

void QnToolTipWidget::setTailWidth(qreal tailWidth) {
    if(qFuzzyCompare(tailWidth, m_tailWidth))
        return;

    m_tailWidth = tailWidth;

    invalidateShape();
}

void QnToolTipWidget::pointTo(const QPointF &pos) {
    QPointF parentTailPos = mapToParent(m_tailPos);
    QPointF parentZeroPos = mapToParent(QPointF(0, 0));
    setPos(pos + parentZeroPos - parentTailPos);
}

QString QnToolTipWidget::text() const {
    return QString();
}

void QnToolTipWidget::setText(const QString &text) {
    return;
}

QRectF QnToolTipWidget::boundingRect() const {
    ensureShape();

    return m_boundingRect;
}

void QnToolTipWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    ensureShape();

    /* Render background. */
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    painter->setPen(QPen(frameBrush(), frameWidth()));
    painter->setBrush(windowBrush());
    painter->drawPath(m_borderShape);

    /* Render text. */
    /*painter->setFont(m_font);
    painter->setPen(m_textPen);
    painter->drawText(QRectF(-m_textSize.width() / 2, -arrowHeight - roundingRadius - padding - m_textSize.height(), m_textSize.width(), m_textSize.height()), Qt::AlignCenter, m_text);*/
}

void QnToolTipWidget::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);

    invalidateShape();
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

    qreal l, t, r, b;
    getContentsMargins(&l, &t, &r, &b);

    Qn::Border tailBorder = this->tailBorder();
    QRectF rect = this->rect();

    /* CCW traversal. */
    QPainterPath borderShape;
    borderShape.moveTo(rect.left(), rect.top() + t);
    addEdgeTo(rect.left(),          rect.bottom() - b,  m_tailPos, m_tailWidth, tailBorder & Qn::LeftBorder, &borderShape);
     addArcTo(rect.left() + l,      rect.bottom(),      &borderShape);
    addEdgeTo(rect.right() - r,     rect.bottom(),      m_tailPos, m_tailWidth, tailBorder & Qn::BottomBorder, &borderShape);
     addArcTo(rect.right(),         rect.bottom() - b,  &borderShape);
    addEdgeTo(rect.right(),         rect.top() + t,     m_tailPos, m_tailWidth, tailBorder & Qn::RightBorder, &borderShape);
     addArcTo(rect.right() - r,     rect.top(),         &borderShape);
    addEdgeTo(rect.left() + l,      rect.top(),         m_tailPos, m_tailWidth, tailBorder & Qn::TopBorder, &borderShape);
     addArcTo(rect.left(),          rect.top() + t,     &borderShape);
    borderShape.closeSubpath();

    m_borderShape = borderShape;
    m_boundingRect = borderShape.boundingRect(); // TODO: add m_borderPen.widthF() / 2
    m_shapeValid = true;
}

