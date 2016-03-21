#include "tool_tip_widget.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QGraphicsLinearLayout>

#include <utils/math/math.h>
#include <utils/common/scoped_painter_rollback.h>

#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/common/geometry.h>
#include <ui/common/palette.h>

namespace  {

    enum { kUseContentMargins = -1};

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
{}

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

QnProxyLabel *QnToolTipWidget::label() const {
    if(layout() && layout()->count() == 1) {
        return dynamic_cast<QnProxyLabel *>(layout()->itemAt(0));
    } else {
        return NULL;
    }
}

QString QnToolTipWidget::text() const {
    QnProxyLabel *label = this->label();
    return label ? label->text() : QString();
}

void QnToolTipWidget::setText(const QString &text) {
    QnProxyLabel *label = this->label();
    if(label) {
        label->setText(text);
        return;
    }

    while(layout() && layout()->count() > 0)
        delete layout()->itemAt(0);

    label = new QnProxyLabel();
    label->setText(text);
    label->setAlignment(Qt::AlignCenter);
    setPaletteColor(label, QPalette::Window, Qt::transparent);

    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addItem(label);
    setLayout(layout);
}

bool QnToolTipWidget::isAutoSize() const {
    return m_autoSize;
}

void QnToolTipWidget::setAutoSize(bool autoSize) {
    m_autoSize = autoSize;
}

QRectF QnToolTipWidget::boundingRect() const {
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

void QnToolTipWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    ensureShape();

    /* Render background. */
    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    painter->setPen(QPen(frameBrush(), frameWidth()));
    painter->setBrush(windowBrush());
    painter->drawPath(m_borderShape);
}

void QnToolTipWidget::updateGeometry() {
    base_type::updateGeometry();

    if(m_autoSize)
        resize(effectiveSizeHint(Qt::PreferredSize));
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

