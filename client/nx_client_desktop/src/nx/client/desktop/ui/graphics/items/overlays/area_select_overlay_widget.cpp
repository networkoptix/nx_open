#include "area_select_overlay_widget.h"

#include <QtGui/QPainter>

#include <ui/processors/drag_processor.h>
#include <ui/processors/drag_process_handler.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/client/desktop/ui/common/painter_transform_scale_stripper.h>

namespace nx {
namespace client {
namespace desktop {

// ------------------------------------------------------------------------------------------------
// AreaSelectOverlayWidget::Private

class AreaSelectOverlayWidget::Private:
    public QObject,
    public DragProcessHandler
{
    AreaSelectOverlayWidget* const q = nullptr;

public:
    Private(AreaSelectOverlayWidget* q);

protected:
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDrag(DragInfo* info) override;

private:
    void updateRect(const DragInfo* info);

public:
    const QScopedPointer<DragProcessor> dragProcessor;
    bool active = false;
    QRectF relativeRect;
};

AreaSelectOverlayWidget::Private::Private(AreaSelectOverlayWidget* q):
    QObject(),
    DragProcessHandler(),
    q(q),
    dragProcessor(new DragProcessor())
{
    dragProcessor->setHandler(this);
}

void AreaSelectOverlayWidget::Private::updateRect(const DragInfo* info)
{
    auto topLeft = info->mousePressItemPos();
    auto bottomRight = info->lastMouseItemPos();

    if (topLeft.x() > bottomRight.x())
        std::swap(topLeft.rx(), bottomRight.rx());

    if (topLeft.y() > bottomRight.y())
        std::swap(topLeft.ry(), bottomRight.ry());

    static const QRectF kUnitRect(0.0, 0.0, 1.0, 1.0);

    relativeRect = nx::client::core::Geometry::toSubRect(q->rect(), QRectF(topLeft, bottomRight))
        .intersected(kUnitRect);

    q->update();
}

void AreaSelectOverlayWidget::Private::startDrag(DragInfo* info)
{
    updateRect(info);
}

void AreaSelectOverlayWidget::Private::dragMove(DragInfo* info)
{
    updateRect(info);
}

void AreaSelectOverlayWidget::Private::finishDrag(DragInfo* info)
{
    updateRect(info);
    emit q->selectedAreaChanged(relativeRect);
}

// ------------------------------------------------------------------------------------------------
// AreaSelectOverlayWidget

AreaSelectOverlayWidget::AreaSelectOverlayWidget(QGraphicsWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setFocusPolicy(Qt::NoFocus);
    setAcceptedMouseButtons(Qt::NoButton);
}

AreaSelectOverlayWidget::~AreaSelectOverlayWidget()
{
}

bool AreaSelectOverlayWidget::active() const
{
    return d->active;
}

void AreaSelectOverlayWidget::setActive(bool value)
{
    if (value == d->active)
        return;

    d->active = value;
    setAcceptedMouseButtons(d->active ? Qt::LeftButton : Qt::NoButton);

    if (d->active)
        setCursor(Qt::CrossCursor);
    else
        unsetCursor();
}

QRectF AreaSelectOverlayWidget::selectedArea() const
{
    return d->relativeRect;
}

void AreaSelectOverlayWidget::clearSelectedArea()
{
    if (d->relativeRect.isEmpty())
        return;

    d->relativeRect = QRectF();
    update();
}

void AreaSelectOverlayWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    if (d->relativeRect.isEmpty())
        return;

    const auto absoluteRect = nx::client::core::Geometry::subRect(rect(), d->relativeRect);

    static const auto kLineWidth = 2;

    QnScopedPainterPenRollback penRollback(painter, QPen(palette().highlight(), kLineWidth));
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);

    const ui::PainterTransformScaleStripper scaleStripper(painter);
    painter->drawRect(scaleStripper.mapRect(absoluteRect));
}

void AreaSelectOverlayWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    d->dragProcessor->mouseMoveEvent(this, event);
}

void AreaSelectOverlayWidget::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    d->dragProcessor->mousePressEvent(this, event);
}

void AreaSelectOverlayWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    d->dragProcessor->mouseReleaseEvent(this, event);
}

} // namespace desktop
} // namespace client
} // namespace nx
