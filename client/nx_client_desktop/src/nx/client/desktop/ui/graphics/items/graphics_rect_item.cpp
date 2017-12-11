#include "graphics_rect_item.h"

#include <utils/common/scoped_painter_rollback.h>
#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>

namespace nx {
namespace client {
namespace desktop {

GraphicsRectItem::GraphicsRectItem(QGraphicsItem* parent):
    base_type(parent)
{
}

void GraphicsRectItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*w*/)
{
    QnScopedPainterPenRollback penRollback(painter, pen());
    QnScopedPainterBrushRollback brushRollback(painter, brush());

    const ui::PainterTransformScaleStripper scaleStripper(painter);

    auto rect = scaleStripper.mapRect(this->rect());
    if ((pen().width() & 1) == 1)
        rect.adjust(0.5, 0.5, -0.5, -0.5);
    painter->drawRect(rect);
}

} // namespace desktop
} // namespace client
} // namespace nx
