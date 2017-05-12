#include "layout_tour_drop_placeholder.h"

#include <QtGui/QPainter>

#include <nx/utils/math/fuzzy.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

LayoutTourDropPlaceholder::LayoutTourDropPlaceholder(
    QGraphicsItem* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags)
{
    setAcceptedMouseButtons(0);
}

QRectF LayoutTourDropPlaceholder::boundingRect() const
{
    return rect();
}

void LayoutTourDropPlaceholder::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    painter->fillRect(m_rect, Qt::red);
}

const QRectF& LayoutTourDropPlaceholder::rect() const
{
    return m_rect;
}

void LayoutTourDropPlaceholder::setRect(const QRectF& rect)
{
    if (qFuzzyEquals(m_rect, rect))
        return;

    prepareGeometryChange();
    m_rect = rect;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
