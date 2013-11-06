#include "selection_item.h"

#include <cassert>

#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/geometry.h>


// -------------------------------------------------------------------------- //
// SelectionItem
// -------------------------------------------------------------------------- //
SelectionItem::SelectionItem(QGraphicsItem *parent):
    base_type(parent),
    m_viewport(NULL)
{
    setAcceptedMouseButtons(0);

    /* Initialize colors with some sensible defaults. Calculations are taken from XP style. */
    QPalette palette = QApplication::style()->standardPalette();
    QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
    setPen(highlight.darker(120));
    setBrush(QColor(
        qMin(highlight.red() / 2 + 110, 255),
        qMin(highlight.green() / 2 + 110, 255),
        qMin(highlight.blue() / 2 + 110, 255),
        127
    ));

    /* Don't disable this item here or it will swallow mouse wheel events. */
}

SelectionItem::~SelectionItem() {
    return;
}

QRectF SelectionItem::rect() const {
    return QRectF(m_origin, m_corner).normalized();
}

void SelectionItem::setRect(const QRectF &rect) {
    setRect(rect.topLeft(), rect.bottomRight());
}

void SelectionItem::setRect(const QPointF &origin, const QPointF &corner) {
    if(m_origin == origin && m_corner == corner)
        return;

    m_origin = origin;
    m_corner = corner;

    QPainterPath path;
    path.addRect(QRectF(origin, corner).normalized());
    setPath(path);
}

const QPointF &SelectionItem::origin() const {
    return m_origin;
}

void SelectionItem::setOrigin(const QPointF &origin) {
    setRect(origin, m_corner);
}

const QPointF &SelectionItem::corner() const {
    return m_corner;
}

void SelectionItem::setCorner(const QPointF &corner) {
    setRect(m_origin, corner);
}

void SelectionItem::setViewport(QWidget *viewport) {
    m_viewport = viewport;
}

QWidget *SelectionItem::viewport() const {
    return m_viewport;
}

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    if (m_viewport && widget != m_viewport)
        return; /* Draw it on source viewport only. */

    base_type::paint(painter, option, widget);
}


// -------------------------------------------------------------------------- //
// FixedArSelectionItem
// -------------------------------------------------------------------------- //
QRectF FixedArSelectionItem::boundingRect() const {
    return QnGeometry::dilated(base_type::boundingRect(), m_elementSize / 2.0);
}

void FixedArSelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    base_type::paint(painter, option, widget);

    if(m_options & (DrawCentralElement | DrawSideElements)) {
        QRectF rect = base_type::rect();
        QPointF center = rect.center();
        if(rect.isEmpty())
            return;

        QPainterPath path;
        qreal elementHalfSize = qMin(m_elementSize, qMin(rect.width(), rect.height()) / 2.0) / 2.0;

        if(m_options & DrawCentralElement)
            path.addEllipse(center, elementHalfSize, elementHalfSize);

        if(m_options & DrawSideElements) {
            foreach(const QPointF &sidePoint, m_sidePoints) {
                QPointF v = sidePoint - center;
                qreal l = QnGeometry::length(v);
                qreal a = -QnGeometry::atan2(v) / M_PI * 180;
                qreal da = 60.0;

                QPointF c = sidePoint - v / l * (elementHalfSize * 1.5);
                QPointF r = QPointF(elementHalfSize, elementHalfSize) * 1.5;
                QRectF rect(c - r, c + r);

                path.arcMoveTo(rect, a - da);
                path.arcTo(rect, a - da, 2 * da);
            }
        }

        QnScopedPainterPenRollback penRollback(painter, QPen(pen().color().lighter(120), elementHalfSize / 2.0));
        QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
        painter->drawPath(path);
    }
}

void FixedArSelectionItem::setGeometry(const QPointF &origin, const QPointF &corner, const qreal aspectRatio, const QRectF &boundingRect) {
    QSizeF extraBoundingSize = QSizeF(boundingRect.height() * aspectRatio, boundingRect.width() / aspectRatio);
    QRectF actualBoundingRect = boundingRect.intersected(QRectF(origin - QnGeometry::toPoint(extraBoundingSize), 2 * extraBoundingSize));
    QPointF boundedCorner = QnGeometry::bounded(corner, actualBoundingRect);

    QRectF rect = QnGeometry::movedInto(
        QnGeometry::expanded(
            aspectRatio,
            QRectF(origin, boundedCorner).normalized(),
            Qt::KeepAspectRatioByExpanding,
            Qt::AlignCenter
        ),
        actualBoundingRect
    );

    QVector<QPointF> sidePoints;
    sidePoints << origin << boundedCorner;

    setRect(rect);
    setSidePoints(sidePoints);
}

