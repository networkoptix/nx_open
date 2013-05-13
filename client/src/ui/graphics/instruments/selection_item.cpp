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

