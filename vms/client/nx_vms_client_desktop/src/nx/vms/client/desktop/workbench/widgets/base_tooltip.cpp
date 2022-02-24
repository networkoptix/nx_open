// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_tooltip.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QWindow>

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

namespace {

struct TailedEdgeHalfGeometry
{
    qreal straightLineLength;
    qreal lesserWidthDecrease;
    qreal arcWidth;
    qreal tailXDiff;

    TailedEdgeHalfGeometry(
        const BaseTooltip::TailGeometry& tailGeometry,
        qreal radius,
        qreal length)
    {
        // This constructor implements complex geometry calculation logic for cases when
        // tail overlaps rounded corner of the edge. Calculated values are used later in
        // BaseTooltip::addTailedEdge().
        const qreal halfTailWidth = tailGeometry.greaterWidth / 2;
        const qreal halfTailWidthDiff = (tailGeometry.greaterWidth - tailGeometry.lesserWidth) / 2;
        straightLineLength = length - halfTailWidth - radius;
        const qreal tailWidthLack = qMax(0.0, -straightLineLength);

        // Tail width never decreased below its half.
        const qreal tailWidthDecrease = qMin(halfTailWidth, tailWidthLack);
        lesserWidthDecrease = qMax(0.0, tailWidthDecrease - halfTailWidthDiff);
        arcWidth = qMax(0.0, radius - (tailWidthLack - tailWidthDecrease));
        tailXDiff = qMax(0.0, halfTailWidthDiff - tailWidthDecrease);
    }
};

Qt::Edge nextEdgeCcw(Qt::Edge edge)
{
    switch (edge)
    {
        case Qt::LeftEdge:
            return Qt::BottomEdge;
        case Qt::TopEdge:
            return Qt::LeftEdge;
        case Qt::RightEdge:
            return Qt::TopEdge;
        case Qt::BottomEdge:
            return Qt::RightEdge;
    }
    return Qt::TopEdge;
}

QPoint edgeOrigin(const QRect& rect, Qt::Edge edge)
{
    switch (edge)
    {
        // bottom(), right(), and derived QRect methods do not work because they return
        // requisite value minus one.
        case Qt::LeftEdge:
            return {rect.x(), rect.y()};
        case Qt::TopEdge:
            return {rect.x() + rect.width(), rect.y()};
        case Qt::RightEdge:
            return {rect.x() + rect.width(), rect.y() + rect.height()};
        case Qt::BottomEdge:
            return {rect.x(), rect.y() + rect.height()};
    }
    return QPoint();
}

QPointF orientedVector(const QPointF& vector, Qt::Edge edge)
{
    switch (edge)
    {
        case Qt::LeftEdge:
            return {-vector.y(), vector.x()};
        case Qt::TopEdge:
            return {-vector.x(), -vector.y()};
        case Qt::RightEdge:
            return {vector.y(), -vector.x()};
        case Qt::BottomEdge:
            return vector;
    }
    return QPointF();
}

/**
 * Add line to path starting from its current position following the vector.
 * @param vector "Not oriented" vector: direction {1, 0} means "forward", while real direction
 *     for line added will be calculated depending on edge.
 * @param edge Edge corresponding to "forward" direction of vector, while bypass direction is CCW.
 *     I.e. vector {1, 0} provided will produce real line direction {1, 0} for Qt::BottomEdge.
 */
void addLine(const QPointF& vector, Qt::Edge edge, QPainterPath* path)
{
    path->lineTo(path->currentPosition() + orientedVector(vector, edge));
}

/**
 * Meaning of arguments is the same as for addLine().
 */
void addArc(const QPointF& vector, Qt::Edge edge, QPainterPath* path)
{
    const QPointF start = path->currentPosition();
    const QPointF end = start + orientedVector(vector, edge);
    const QRectF quarter = QRectF(start, end).normalized();
    QRectF rect = quarter;

    qreal startAngle;
    if (start.x() < end.x())
    {
        if (start.y() < end.y())
        {
            startAngle = 180;
            rect.setTop(quarter.top() - quarter.height());
            rect.setRight(quarter.right() + quarter.width());
        }
        else
        {
            startAngle = 270;
            rect.setTop(quarter.top() - quarter.height());
            rect.setLeft(quarter.left() - quarter.width());
        }
    }
    else
    {
        if (start.y() < end.y())
        {
            startAngle = 90;
            rect.setBottom(quarter.bottom() + quarter.height());
            rect.setRight(quarter.right() + quarter.width());
        }
        else
        {
            startAngle = 0;
            rect.setBottom(quarter.bottom() + quarter.height());
            rect.setLeft(quarter.left() - quarter.width());
        }
    }

    path->arcTo(rect, startAngle, 90);
}

} // namespace

BaseTooltip::BaseTooltip(
    Qt::Edge tailBorder,
    bool filterEvents /*= true*/,
    QWidget* parent /*= nullptr*/)
    :
    m_tailBorder(tailBorder),
    m_parentWidget(parent)
{
    if (parent)
        connect(parent, &QObject::destroyed, this, &QObject::deleteLater);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    if (filterEvents)
    {
        installEventHandler({this}, {QEvent::Resize, QEvent::Move},
            this, &BaseTooltip::moveToPointedPos);

        initOpacityMixin();
    }

    setAttribute(Qt::WA_ShowWithoutActivating);

    const auto flags = windowFlags();
    setWindowFlags(
        flags
        | Qt::Tool
        | Qt::FramelessWindowHint
        | Qt::NoDropShadowWindowHint);

    // Create native window handler.
    winId();
}

void BaseTooltip::setTransparentForInput(bool enabled)
{
    auto flags = windowFlags();
    flags.setFlag(Qt::WindowTransparentForInput, enabled);
    setWindowFlags(flags);
}

void BaseTooltip::pointTo(const QPoint& pos, const QRect& longitudinalBounds /* = QRect()*/)
{
    if (m_parentWidget)
    {
        if (m_parentWidget->window())
        {
            if (const auto parentWindow = m_parentWidget->window()->windowHandle())
            {
                if (parentWindow != window()->windowHandle()->transientParent())
                    window()->windowHandle()->setTransientParent(parentWindow);
            }
        }

        m_pointsTo = m_parentWidget->mapToGlobal(pos);
        m_longitudinalBounds = QRect(
            m_parentWidget->mapToGlobal(longitudinalBounds.topLeft()),
            m_parentWidget->mapToGlobal(longitudinalBounds.bottomRight()));
    }
    else
    {
        m_pointsTo = pos;
        m_longitudinalBounds = longitudinalBounds;
    }
    moveToPointedPos();
}

QPoint BaseTooltip::pointedTo() const
{
    return m_pointsTo;
}

Qt::Edge BaseTooltip::tailBorder() const
{
    return m_tailBorder;
}

void BaseTooltip::setTailBorder(Qt::Edge tailBorder)
{
    m_tailBorder = tailBorder;
}

BaseTooltip::TailGeometry BaseTooltip::tailGeometry() const
{
    return m_tailGeometry;
}

void BaseTooltip::setTailGeometry(const TailGeometry& geometry)
{
    m_tailGeometry = geometry;
}

QRect BaseTooltip::bodyRect() const
{
    QRect rect = contentsRect();
    switch (m_tailBorder)
    {
        case Qt::LeftEdge:
            rect.setX(rect.x() + m_tailGeometry.height);
            break;
        case Qt::TopEdge:
            rect.setY(rect.y() + m_tailGeometry.height);
            break;
        case Qt::RightEdge:
            rect.setWidth(rect.width() - m_tailGeometry.height);
            break;
        case Qt::BottomEdge:
            rect.setHeight(rect.height() - m_tailGeometry.height);
            break;
    }
    return rect;
}

bool BaseTooltip::pointsVertically() const
{
    return m_tailBorder == Qt::TopEdge || m_tailBorder == Qt::BottomEdge;
}

int BaseTooltip::tailEdgeLength() const
{
    return pointsVertically() ? contentsRect().width() : contentsRect().height();
}

int BaseTooltip::tailOffsetFromCenter() const
{
    if (m_longitudinalBounds.isEmpty())
        return 0;

    if (pointsVertically())
    {
        const int xWithoutOffset = m_pointsTo.x() - contentsRect().width() / 2;
        const int xWithOffset = qBound(
            m_longitudinalBounds.left(),
            xWithoutOffset,
            m_longitudinalBounds.right() - contentsRect().width());
        return xWithoutOffset - xWithOffset;
    }

    const int yWithoutOffset = m_pointsTo.y() - contentsRect().height() / 2;
    const int yWithOffset = qBound(
        m_longitudinalBounds.top(),
        yWithoutOffset,
        m_longitudinalBounds.bottom() - contentsRect().height());
    return yWithoutOffset - yWithOffset;
}

int BaseTooltip::tailOffset() const
{
    if (pointsVertically())
        return contentsRect().width() / 2 + tailOffsetFromCenter();

    return contentsRect().height() / 2 + tailOffsetFromCenter();
}

int BaseTooltip::notOrientedTailOffset() const
{
    const int offset = tailOffset();

    switch (m_tailBorder)
    {
        case Qt::LeftEdge:
        case Qt::BottomEdge:
            return offset;

        case Qt::TopEdge:
            return contentsRect().width() - offset;

        case Qt::RightEdge:
            return contentsRect().height() - offset;
    }
    return 0;
}

void BaseTooltip::setRoundingRadius(int radius)
{
    m_roundingRadius = radius;
}

void BaseTooltip::colorizeBorderShape(const QPainterPath& borderShape)
{
    QPainter(this).fillPath(borderShape, QBrush(colorTheme()->color("tooltip.background")));
}

void BaseTooltip::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_clicked = true;
}

void BaseTooltip::mouseMoveEvent(QMouseEvent *event)
{
    if (!contentsRect().contains(event->pos()))
        m_clicked = false;
}

void BaseTooltip::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_clicked)
        emit clicked();

    m_clicked = false;
}

void BaseTooltip::moveToPointedPos()
{
    if (m_movingProcess)
        return;

    m_movingProcess = true;

    QRect geometry = this->geometry().marginsRemoved(contentsMargins());
    switch (m_tailBorder)
    {
        case Qt::LeftEdge:
            geometry.moveTo(
                m_pointsTo.x(),
                m_pointsTo.y() - contentsRect().height() / 2 - tailOffsetFromCenter());
            break;
        case Qt::RightEdge:
            geometry.moveTo(
                m_pointsTo.x() - contentsRect().width(),
                m_pointsTo.y() - contentsRect().height() / 2 - tailOffsetFromCenter());
            break;
        case Qt::TopEdge:
            geometry.moveTo(
                m_pointsTo.x() - contentsRect().width() / 2 - tailOffsetFromCenter(),
                m_pointsTo.y());
            break;
        case Qt::BottomEdge:
            geometry.moveTo(
                m_pointsTo.x() - contentsRect().width() / 2 - tailOffsetFromCenter(),
                m_pointsTo.y() - contentsRect().height());
            break;
    }
    setGeometry(geometry.marginsAdded(contentsMargins()));
    update();

    m_movingProcess = false;
}

void BaseTooltip::addTailedEdge(QPainterPath* path) const
{
    // Terms "x", "y", "width", and "height" mention not oriented edge coordinate
    // system, i.e. they are literal only for Qt::BottomEdge.
    const qreal tailOffset = notOrientedTailOffset();
    const qreal radius = m_roundingRadius;
    const qreal height = m_tailGeometry.height;
    const TailedEdgeHalfGeometry pre(m_tailGeometry, radius, tailOffset);
    const TailedEdgeHalfGeometry post(m_tailGeometry, radius, tailEdgeLength() - tailOffset);

    // Add pre-tail arc and the line co-directional for the edge.
    addArc({pre.arcWidth, radius}, m_tailBorder, path);
    if (pre.straightLineLength > 0)
        addLine({pre.straightLineLength, 0}, m_tailBorder, path);

    // Add tail trapezoid.
    addLine({pre.tailXDiff, height}, m_tailBorder, path);
    addLine(
        {m_tailGeometry.lesserWidth - pre.lesserWidthDecrease - post.lesserWidthDecrease, 0},
        m_tailBorder, path);
    addLine({post.tailXDiff, -height}, m_tailBorder, path);

    // Add post-tail line co-directional for the edge and the arc.
    if (post.straightLineLength > 0)
        addLine({post.straightLineLength, 0}, m_tailBorder, path);
    addArc({post.arcWidth, -radius}, m_tailBorder, path);
}

void BaseTooltip::paintEvent(QPaintEvent*)
{
    const QRect rect = bodyRect();
    const qreal radius = m_roundingRadius;

    // Add edges one by one CCW starting from the one containing tail.
    QPainterPath borderShape;
    Qt::Edge edge = m_tailBorder;
    borderShape.moveTo(
        edgeOrigin(rect, edge) + orientedVector({0, -radius}, edge));

    addTailedEdge(&borderShape); //< Two arcs (pre-edge and post-edge) included.

    // Add line for the edge "next" (perpendicular) to tailed one.
    const int nonTailEdgeLength = pointsVertically()? rect.height() : rect.width();
    edge = nextEdgeCcw(edge);
    addLine({nonTailEdgeLength - 2 * radius, 0}, edge, &borderShape);

    // Add two arcs and line for the edge parallel to tailed one.
    edge = nextEdgeCcw(edge);
    addArc({radius, radius}, edge, &borderShape);
    addLine({tailEdgeLength() - 2 * radius, 0}, edge, &borderShape);
    addArc({radius, -radius}, edge, &borderShape);

    // Last edge line added by closing the subpath.
    borderShape.closeSubpath();

    colorizeBorderShape(borderShape);
}

} // namespace nx::vms::client::desktop
