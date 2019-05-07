#include "area_select_overlay_widget.h"

#include <QtGui/QPainter>

#include <ui/processors/drag_processor.h>
#include <ui/processors/drag_process_handler.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// AreaSelectOverlayWidget::Private

class AreaSelectOverlayWidget::Private:
    public QObject,
    public DragProcessHandler
{
    AreaSelectOverlayWidget* const q = nullptr;

public:
    Private(AreaSelectOverlayWidget* q, QnResourceWidget* resourceWidget);

protected:
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDrag(DragInfo* info) override;

private:
    void updateRect(const DragInfo* info);

public:
    const QScopedPointer<DragProcessor> dragProcessor;
    const QPointer<QnResourceWidget> resourceWidget;
    bool active = false;
    QRectF relativeRect;
};

AreaSelectOverlayWidget::Private::Private(
    AreaSelectOverlayWidget* q,
    QnResourceWidget* resourceWidget)
    :
    QObject(),
    DragProcessHandler(),
    q(q),
    dragProcessor(new DragProcessor()),
    resourceWidget(resourceWidget)
{
    dragProcessor->setHandler(this);

    QObject::connect(this->resourceWidget.data(), &QnResourceWidget::selectionStateChanged, this,
        [this]() { this->q->update(); });
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

    relativeRect = nx::vms::client::core::Geometry::toSubRect(q->rect(), QRectF(topLeft, bottomRight))
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
    emit q->selectedAreaChanged(relativeRect, {});
}

//-------------------------------------------------------------------------------------------------
// AreaSelectOverlayWidget

AreaSelectOverlayWidget::AreaSelectOverlayWidget(
    QGraphicsWidget* parent,
    QnResourceWidget* resourceWidget)
    :
    base_type(parent),
    d(new Private(this, resourceWidget))
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

void AreaSelectOverlayWidget::setSelectedArea(const QRectF& value)
{
    if (d->relativeRect == value)
        return;

    d->relativeRect = value;
    emit selectedAreaChanged(d->relativeRect, {});

    update();
}

void AreaSelectOverlayWidget::clearSelectedArea()
{
    setSelectedArea(QRectF());
}

void AreaSelectOverlayWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    if (!d->resourceWidget || d->relativeRect.isEmpty())
        return;

    switch (d->resourceWidget->selectionState())
    {
        case QnResourceWidget::SelectionState::focused:
        case QnResourceWidget::SelectionState::focusedAndSelected:
        case QnResourceWidget::SelectionState::inactiveFocused:
            break;

        default:
            return;
    }

    const auto absoluteRect = nx::vms::client::core::Geometry::subRect(rect(), d->relativeRect);

    static const auto kLineWidth = 2;

    QnScopedPainterPenRollback penRollback(painter, QPen(palette().highlight(), kLineWidth));
    QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);

    const PainterTransformScaleStripper scaleStripper(painter);
    painter->drawRect(scaleStripper.mapRect(absoluteRect));
}

void AreaSelectOverlayWidget::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    d->dragProcessor->mouseMoveEvent(this, event);
}

void AreaSelectOverlayWidget::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit selectionStarted({});
    d->dragProcessor->mousePressEvent(this, event);
}

void AreaSelectOverlayWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    d->dragProcessor->mouseReleaseEvent(this, event);
}

} // namespace nx::vms::client::desktop
