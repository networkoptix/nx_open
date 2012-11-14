#include "selection_item.h"

#include <cassert>

#include <QtGui/QApplication>
#include <QtGui/QStyle>

#include <utils/common/scoped_painter_rollback.h>

SelectionItem::SelectionItem(QGraphicsItem *parent):
    base_type(parent),
    m_viewport(NULL)
{
    setAcceptedMouseButtons(0);

    /* Initialize colors with some sensible defaults. Calculations are taken from XP style. */
    QPalette palette = QApplication::style()->standardPalette();
    QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
    m_colors[Border] = highlight.darker(120);
    m_colors[Base] = QColor(
        qMin(highlight.red() / 2 + 110, 255),
        qMin(highlight.green() / 2 + 110, 255),
        qMin(highlight.blue() / 2 + 110, 255),
        127
    );

    /* Don't disable this item here or it will swallow mouse wheel events. */
}

SelectionItem::~SelectionItem() {
    return;
}

QRectF SelectionItem::boundingRect() const {
    return QRectF(m_origin, m_corner).normalized();
}

void SelectionItem::setBoundingRect(const QRectF &boundingRect) {
    prepareGeometryChange();

    m_origin = boundingRect.topLeft();
    m_corner = boundingRect.bottomRight();
}

void SelectionItem::setBoundingRect(const QPointF &origin, const QPointF &corner) {
    prepareGeometryChange();

    m_origin = origin;
    m_corner = corner;
}

const QPointF &SelectionItem::origin() const {
    return m_origin;
}

void SelectionItem::setOrigin(const QPointF &origin) {
    prepareGeometryChange();

    m_origin = origin;
}

const QPointF &SelectionItem::corner() const {
    return m_corner;
}

void SelectionItem::setCorner(const QPointF &corner) {
    prepareGeometryChange();

    m_corner = corner;
}

const QColor &SelectionItem::color(ColorRole colorRole) const {
    assert(colorRole >= 0 && colorRole < ColorRoleCount);

    return m_colors[colorRole];
}

void SelectionItem::setColor(ColorRole colorRole, const QColor &color) {
    assert(colorRole >= 0 && colorRole < ColorRoleCount);

    m_colors[colorRole] = color;

    update();
}

void SelectionItem::setViewport(QWidget *viewport) {
    m_viewport = viewport;
}

QWidget *SelectionItem::viewport() const {
    return m_viewport;
}

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    if (m_viewport && widget != m_viewport)
        return; /* Draw it on source viewport only. */

    /* Draw selection on cells */
    QRectF rect = QRectF(m_origin, m_corner).normalized();
    if(!rect.isEmpty()) {
        QnScopedPainterPenRollback penRollback(painter, m_colors[Border]);
        QnScopedPainterBrushRollback brushRollback(painter, m_colors[Base]);
        painter->drawRect(rect);
    }
}

