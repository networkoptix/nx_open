#include "area_highlight_overlay_widget.h"

#include <QtCore/QVector>
#include <QtGui/QPainter>

#include <QtWidgets/QGraphicsSceneHoverEvent>

#include <ui/common/fixed_rotation.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <nx/client/core/utils/geometry.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include "area_rect_item.h"
#include "area_tooltip_item.h"

using nx::vms::client::core::Geometry;

namespace nx::vms::client::desktop {

namespace {

static QRectF calculateLabelGeometry(
    QRectF boundingRect,
    QSizeF labelSize,
    QMarginsF labelMargins,
    QRectF objectRect)
{
    constexpr qreal kTitleAreaHeight = 24;
    boundingRect = boundingRect.adjusted(0, kTitleAreaHeight, 0, 0);
    labelSize = Geometry::bounded(labelSize, boundingRect.size());

    const QMarginsF space(
        objectRect.left() - boundingRect.left(),
        objectRect.top() - boundingRect.top(),
        boundingRect.right() - objectRect.right(),
        boundingRect.bottom() - objectRect.bottom());

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
    checkSide(Qt::RightEdge, labelSize.width(), space.right());
    checkSide(Qt::TopEdge, labelSize.height(), space.top());
    checkSide(Qt::BottomEdge, labelSize.height(), space.bottom());

    static constexpr qreal kLabelPadding = 2.0;
    QRectF availableRect = Geometry::dilated(boundingRect.intersected(objectRect), kLabelPadding);

    if (bestSpaceProportion < 1.0
        && availableRect.width() / labelSize.width() > bestSpaceProportion
        && availableRect.height() / labelSize.height() > bestSpaceProportion)
    {
        labelSize = Geometry::bounded(labelSize, availableRect.size());
        return Geometry::aligned(labelSize, availableRect, Qt::AlignTop | Qt::AlignRight);
    }

    QPointF pos;

    switch (bestSide)
    {
        case Qt::LeftEdge:
            pos = QPointF(
                objectRect.left() - labelSize.width(),
                objectRect.top() - labelMargins.top());
            availableRect = QRectF(
                boundingRect.left(), boundingRect.top(),
                space.left(), boundingRect.height());
            break;
        case Qt::RightEdge:
            pos = QPointF(
                objectRect.right(),
                objectRect.top() - labelMargins.top());
            availableRect = QRectF(
                objectRect.right(), boundingRect.top(),
                space.right(), boundingRect.height());
            break;
        case Qt::TopEdge:
            pos = QPointF(
                objectRect.left() - labelMargins.left(),
                objectRect.top() - labelSize.height());
            availableRect = QRectF(
                boundingRect.left(), boundingRect.top(),
                boundingRect.width(), space.top());
            break;
        case Qt::BottomEdge:
            pos = QPointF(
                objectRect.left() - labelMargins.left(),
                objectRect.bottom());
            availableRect = QRectF(
                boundingRect.left(), std::max(objectRect.bottom(), boundingRect.top()),
                boundingRect.width(), space.bottom());
            break;
    }

    labelSize = Geometry::bounded(labelSize, availableRect.size());
    return Geometry::movedInto(QRectF(pos, labelSize), availableRect);
}

static QColor calculateTooltipColor(const QColor& frameColor)
{
    static constexpr int kTooltipBackgroundLightness = 10;
    static constexpr int kTooltipBackgroundAlpha = 127;

    auto color = frameColor.toHsl();
    color = QColor::fromHsl(
        color.hslHue(),
        color.hslSaturation(),
        kTooltipBackgroundLightness,
        kTooltipBackgroundAlpha);
    return color;
}

AreaTooltipItem::Fonts makeFonts(const QFont& baseFont)
{
    AreaTooltipItem::Fonts result;
    result.name = baseFont;
    result.name.setPixelSize(11);

    result.value = baseFont;
    result.value.setPixelSize(11);
    result.value.setWeight(QFont::Medium);

    result.title = baseFont;
    result.title.setPixelSize(13);
    result.title.setWeight(QFont::Medium);

    return result;
}

} // namespace

class AreaHighlightOverlayWidget::Private: public QObject
{
    AreaHighlightOverlayWidget* const q = nullptr;

public:
    struct Area
    {
        AreaHighlightOverlayWidget::AreaInformation info;
        QSharedPointer<AreaRectItem> rectItem;
        QSharedPointer<AreaTooltipItem> tooltipItem;
        QRectF rotatedRectangle;

        QRectF actualRect(const QSizeF& widgetSize) const
        {
            // Using .toRect() to round rectangle coordinates.
            return Geometry::cwiseMul(rotatedRectangle, widgetSize).toRect();
        }
    };

    Private(AreaHighlightOverlayWidget* q);

    void setHighlightedArea(const QnUuid& areaId);
    void updateArea(Area& area);
    void updateArea(const QnUuid& areaId);
    void updateAreas();
    void ensureRotation();
    QRectF rotatedRectangle(const QRectF& source) const;

public:
    QHash<QnUuid, Area> areaById;
    QnUuid highlightedAreaId;
    bool highlightAreasOnHover = true;
    QPointer<QGraphicsRotation> rotation;
    int angle = 0;
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

    updateArea(oldHighlightedAreaId);
    updateArea(areaId);

    q->update();
}

void AreaHighlightOverlayWidget::Private::updateArea(
    AreaHighlightOverlayWidget::Private::Area& area)
{
    if (!area.rectItem || !area.tooltipItem)
        return;

    area.rotatedRectangle = rotatedRectangle(area.info.rectangle);

    const bool highlighted = highlightedAreaId == area.info.id;

    const auto& rect = area.actualRect(q->size());
    area.rectItem->setRect(rect);
    area.rectItem->setFlag(QGraphicsItem::ItemStacksBehindParent, !highlighted);

    QPen pen(area.info.color, highlighted ? 2 : 1);
    pen.setJoinStyle(Qt::MiterJoin);
    area.rectItem->setPen(pen);

    area.tooltipItem->setFlag(QGraphicsItem::ItemStacksBehindParent, !highlighted);
    area.tooltipItem->setText(highlighted ? area.info.hoverText : area.info.text);

    const auto& naturalTooltipSize = area.tooltipItem->boundingRect().size();
    const auto& tooltipGeometry = calculateLabelGeometry(
        q->rect(), naturalTooltipSize, area.tooltipItem->textMargins(), rect);
    area.tooltipItem->setPos(tooltipGeometry.topLeft());
    area.tooltipItem->setScale(
        std::min(1.0, Geometry::scaleFactor(naturalTooltipSize, tooltipGeometry.size())));
    area.tooltipItem->setTargetObjectGeometry(rect);
    area.tooltipItem->setBackgroundColor(
        (Geometry::eroded(rect, 0.5).contains(tooltipGeometry.center()) || !highlighted)
            ? calculateTooltipColor(area.info.color)
            : QColor());
}

void AreaHighlightOverlayWidget::Private::updateArea(const QnUuid& areaId)
{
    if (areaId.isNull())
        return;

    const auto it = areaById.find(areaId);
    if (it == areaById.end())
        return;

    updateArea(*it);
}

void AreaHighlightOverlayWidget::Private::updateAreas()
{
    for (auto& area: areaById)
        updateArea(area);
}

void AreaHighlightOverlayWidget::Private::ensureRotation()
{
    if (rotation)
        return;

    for (auto transform: q->transformations())
    {
        rotation = qobject_cast<QnFixedRotationTransform*>(transform);
        if (rotation)
            break;
    }

    if (!rotation)
        return;

    const auto updateAngle =
        [this]()
        {
            angle = this->rotation
                ? int(Rotation::closestStandardRotation(this->rotation->angle()).value())
                : 0;

            updateAreas();
        };

    connect(rotation.data(), &QGraphicsRotation::angleChanged, this, updateAngle);
    updateAngle();
}

QRectF AreaHighlightOverlayWidget::Private::rotatedRectangle(const QRectF& source) const
{
    switch (angle)
    {
        case 90:
            return QRectF(source.top(), 1.0 - source.right(), source.height(), source.width());

        case 180:
            return QRectF(
                1.0 - source.right(), 1.0 - source.bottom(), source.width(), source.height());

        case 270:
            return QRectF(1.0 - source.bottom(), source.left(), source.height(), source.width());

        default:
            NX_ASSERT(angle == 0);
            return source;
    }
}

AreaHighlightOverlayWidget::AreaHighlightOverlayWidget(QGraphicsWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFocusPolicy(Qt::NoFocus);

    connect(this, &QGraphicsWidget::geometryChanged, d.data(), &Private::updateAreas);
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

    d->ensureRotation();

    auto& area = d->areaById[areaInformation.id];

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

        connect(rectItem.data(), &AreaRectItem::clicked, this,
            [this, id = area.info.id]()
            {
                emit areaClicked(id);
            });
    }

    auto& tooltipItem = area.tooltipItem;
    if (!tooltipItem)
    {
        tooltipItem.reset(new AreaTooltipItem(this));
        tooltipItem->setTextColor(colorTheme()->color("light1"));
        tooltipItem->setFonts(makeFonts(font()));
        tooltipItem->stackBefore(rectItem.data());
    }

    d->updateArea(area);
    NX_VERBOSE(this, "Area was added or updated, total %1", d->areaById.size());

    update();
}

void AreaHighlightOverlayWidget::removeArea(const QnUuid& areaId)
{
    if (d->areaById.remove(areaId) > 0)
    {
        NX_VERBOSE(this, "Area was removed, total %1 left", d->areaById.size());
        update();
    }
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
    const auto dimmerColor = colorTheme()->color("dark1", 0.5);

    const auto it = d->areaById.find(d->highlightedAreaId);
    if (it == d->areaById.end())
        return;

    auto& highlightedArea = *it;

    const auto& widgetSize = size();
    const QRectF rect = highlightedArea.actualRect(widgetSize);

    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    QnScopedPainterBrushRollback brushRollback(painter, dimmerColor);

    painter->drawRect(QRectF(
        0, 0, rect.left(), widgetSize.height()));
    painter->drawRect(QRectF(
        rect.right(), 0, widgetSize.width() - rect.right(), widgetSize.height()));
    painter->drawRect(QRectF(
        rect.left(), 0, rect.width(), rect.top()));
    painter->drawRect(QRectF(
        rect.left(), rect.bottom(), rect.width(), widgetSize.height() - rect.bottom()));
}

bool AreaHighlightOverlayWidget::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::FontChange:
        {
            const auto& font = this->font();
            for (auto& area: d->areaById)
                area.tooltipItem->setFonts(makeFonts(font));
            break;
        }

        default:
            break;
    }

    return base_type::event(event);
}

} // namespace nx::vms::client::desktop
