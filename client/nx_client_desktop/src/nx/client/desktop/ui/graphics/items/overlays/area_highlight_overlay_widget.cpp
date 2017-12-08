#include "area_highlight_overlay_widget.h"

#include <QtCore/QVector>
#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsSceneHoverEvent>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <nx/client/core/utils/geometry.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/client/desktop/ui/common/color_theme.h>
#include <nx/client/desktop/ui/graphics/painters/highlighted_area_text_painter.h>

#include "area_rect_item.h"

using nx::client::core::Geometry;

namespace nx {
namespace client {
namespace desktop {

namespace {

static const auto kDimmerColor = QColor("#70000000");
static constexpr auto kSpacing = 8;

static QRectF calculateLabelGeometry(
    const QRectF& boundingRect,
    const QSizeF& labelSize,
    const QRectF& objectRect,
    const qreal spacing)
{
    const QMarginsF space(
        objectRect.left() - boundingRect.left() - spacing,
        objectRect.top() - boundingRect.top() - spacing,
        boundingRect.right() - objectRect.right() - spacing,
        boundingRect.bottom() - objectRect.bottom() - spacing);

    auto bestSide = Qt::BottomEdge;
    qreal bestSpaceProportion = 0;

    auto checkSide =
        [&](Qt::Edge side, qreal labelSize, qreal space)
        {
            const qreal proportion = space / labelSize;
            if (proportion >= 1.0 || proportion >= bestSpaceProportion)
            {
                bestSpaceProportion = proportion;
                bestSide = side;
            }
        };

    checkSide(Qt::LeftEdge, labelSize.width(), space.left());
    checkSide(Qt::TopEdge, labelSize.height(), space.top());
    checkSide(Qt::RightEdge, labelSize.width(), space.right());
    checkSide(Qt::BottomEdge, labelSize.height(), space.bottom());

    QPointF pos;
    QSizeF size;

    switch (bestSide)
    {
        case Qt::LeftEdge:
            pos = QPointF(
                objectRect.left() - labelSize.width() - spacing,
                objectRect.top());
            size = Geometry::bounded(labelSize, QSizeF(space.left(), boundingRect.height()));
            break;
        case Qt::RightEdge:
            pos = QPointF(
                objectRect.right() + spacing,
                objectRect.top());
            size = Geometry::bounded(labelSize, QSizeF(space.right(), boundingRect.height()));
            break;
        case Qt::TopEdge:
            pos = QPointF(
                objectRect.left(),
                objectRect.top() - labelSize.height() - spacing);
            size = Geometry::bounded(labelSize, QSizeF(boundingRect.width(), space.top()));
            break;
        case Qt::BottomEdge:
            pos = QPointF(
                objectRect.left(),
                objectRect.bottom() + spacing);
            size = Geometry::bounded(labelSize, QSizeF(boundingRect.width(), space.bottom()));
            break;
    }

    return Geometry::movedInto(QRectF(pos, size), boundingRect);
}

} // namespace

class AreaHighlightOverlayWidget::Private
{
    AreaHighlightOverlayWidget* const q = nullptr;

public:
    struct Area
    {
        AreaHighlightOverlayWidget::AreaInformation info;
        QPixmap textPixmap;
        QSharedPointer<AreaRectItem> rectItem;

        void invalidatePixmap()
        {
            textPixmap = QPixmap();
        }

        QRectF actualRect(const QSizeF& widgetSize) const
        {
            // Using .toRect() to round rectangle coordinates.
            return Geometry::cwiseMul(info.rectangle, widgetSize).toRect();
        }
    };

    Private(AreaHighlightOverlayWidget* q);

    void setHighlightedArea(const QnUuid& areaId);
    void invalidatePixmaps();
    void updateAreaRectangle(const Area& area);
    void updateAreaRectangle(const QnUuid& areaId);

public:
    QHash<QnUuid, Area> areaById;
    QnUuid highlightedAreaId;
    bool highlightAreasOnHover = true;
    HighlightedAreaTextPainter textPainter;
};

AreaHighlightOverlayWidget::Private::Private(AreaHighlightOverlayWidget* q):
    q(q)
{
}

void AreaHighlightOverlayWidget::Private::setHighlightedArea(const QnUuid& areaId)
{
    if (areaId == highlightedAreaId)
        return;

    const auto oldHighlightedAreaId = highlightedAreaId;
    highlightedAreaId = areaId;
    emit q->highlightedAreaChanged(areaId);

    updateAreaRectangle(oldHighlightedAreaId);
    updateAreaRectangle(areaId);

    q->update();
}

void AreaHighlightOverlayWidget::Private::invalidatePixmaps()
{
    for (auto& area: areaById)
        area.invalidatePixmap();
}

void AreaHighlightOverlayWidget::Private::updateAreaRectangle(
    const AreaHighlightOverlayWidget::Private::Area& area)
{
    if (!area.rectItem)
        return;

    const bool highlighted = highlightedAreaId == area.info.id;

    area.rectItem->setRect(area.actualRect(q->size()));
    area.rectItem->setFlag(QGraphicsItem::ItemStacksBehindParent, !highlighted);

    QPen pen(area.info.color, highlighted ? 2 : 1);
    pen.setJoinStyle(Qt::MiterJoin);
    area.rectItem->setPen(pen);
}

void AreaHighlightOverlayWidget::Private::updateAreaRectangle(const QnUuid& areaId)
{
    if (areaId.isNull())
        return;

    const auto it = areaById.find(areaId);
    if (it == areaById.end())
        return;

    updateAreaRectangle(*it);
}

AreaHighlightOverlayWidget::AreaHighlightOverlayWidget(QGraphicsWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFocusPolicy(Qt::NoFocus);

    QFont font = this->font();
    font.setPixelSize(11);
    setFont(font);

    d->textPainter.setColor(ColorTheme::instance()->color(lit("light1")));
    d->textPainter.setFont(font);
}

AreaHighlightOverlayWidget::~AreaHighlightOverlayWidget()
{
}

void AreaHighlightOverlayWidget::addOrUpdateArea(
    const AreaHighlightOverlayWidget::AreaInformation& areaInformation)
{
    if (areaInformation.id.isNull())
    {
        NX_ASSERT(false, "Area ID cannot be null.");
        return;
    }

    auto& area = d->areaById[areaInformation.id];
    if (area.info.text != areaInformation.text)
        area.invalidatePixmap();

    area.info = areaInformation;

    auto& rectItem = area.rectItem;
    if (!rectItem)
    {
        rectItem.reset(new AreaRectItem(this));

        connect(rectItem.data(), &AreaRectItem::containsMouseChanged, this,
            [this, id = area.info.id](bool containsMouse)
            {
                if (!d->highlightAreasOnHover)
                    return;

                if (containsMouse)
                    d->setHighlightedArea(id);
                else if (d->highlightedAreaId == id)
                    d->setHighlightedArea(QnUuid());
            });
    }
    d->updateAreaRectangle(area);

    update();
}

void AreaHighlightOverlayWidget::removeArea(const QnUuid& areaId)
{
    if (d->areaById.remove(areaId) > 0)
        update();
}

QnUuid AreaHighlightOverlayWidget::highlightedArea() const
{
    return d->highlightedAreaId;
}

void AreaHighlightOverlayWidget::setHighlightedArea(const QnUuid& areaId)
{
    d->setHighlightedArea(areaId);
    d->highlightAreasOnHover = areaId.isNull();
}

void AreaHighlightOverlayWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*w*/)
{
    const auto it = d->areaById.find(d->highlightedAreaId);
    if (it == d->areaById.end())
        return;

    auto& highlightedArea = *it;

    const auto& widgetSize = size();
    const QRectF rect = highlightedArea.actualRect(widgetSize);

    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    QnScopedPainterBrushRollback brushRollback(painter, kDimmerColor);

    painter->drawRect(QRectF(
        0, 0, rect.left(), widgetSize.height()));
    painter->drawRect(QRectF(
        rect.right(), 0, widgetSize.width() - rect.right(), widgetSize.height()));
    painter->drawRect(QRectF(
        rect.left(), 0, rect.width(), rect.top()));
    painter->drawRect(QRectF(
        rect.left(), rect.bottom(), rect.width(), widgetSize.height() - rect.bottom()));

    if (highlightedArea.textPixmap.isNull())
        highlightedArea.textPixmap = d->textPainter.paintText(highlightedArea.info.text);

    if (highlightedArea.textPixmap.isNull())
        return;

    const auto& pixmapGeometry = calculateLabelGeometry(
        this->rect(), highlightedArea.textPixmap.size(), rect, kSpacing);

    paintPixmapSharp(painter, highlightedArea.textPixmap, pixmapGeometry);
}

bool AreaHighlightOverlayWidget::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::FontChange:
            d->textPainter.setFont(font());
            d->invalidatePixmaps();
            break;

        default:
            break;
    }

    return base_type::event(event);
}

} // namespace desktop
} // namespace client
} // namespace nx
