// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_overlay_widget.h"

#include <QtCore/QVector>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneHoverEvent>

#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <ui/common/fixed_rotation.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/scoped_painter_rollback.h>

#include "area_tooltip_item.h"
#include "figure/box.h"
#include "figure/figure_item.h"
#include "figure/renderer.h"

namespace {

using namespace std::chrono;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::figure;
using namespace nx::vms::client::core;

static constexpr int kAreaLineWidth = 1;
static constexpr bool kNoFillForObjects = false;

static constexpr int kFpsLimit = 60;
static constexpr milliseconds kTimeBetweenUpdateRequests = 1000ms / kFpsLimit;
static constexpr qreal kMinimalTooltipOffset = 16;

struct BestSide
{
    Qt::Edge edge = static_cast<Qt::Edge>(0);
    qreal proportion = 0;
};

bool hasHorizontalSpace(const QMarginsF& space)
{
    return space.left() > kMinimalTooltipOffset && space.right() > kMinimalTooltipOffset;
}

bool hasVerticalSpace(const QMarginsF& space)
{
    return space.top() > kMinimalTooltipOffset && space.bottom() > kMinimalTooltipOffset;
}

/** Tries to find the appropriate side to which place the tooltip. */
BestSide findBestSide(
    const QSizeF& labelSize,
    const QMarginsF& space)
{
    if (space.bottom() - kMinimalTooltipOffset >= labelSize.height() && hasHorizontalSpace(space))
        return {Qt::BottomEdge, /*proportion*/ 1};
    else if (space.top() - kMinimalTooltipOffset >= labelSize.height() && hasHorizontalSpace(space))
        return {Qt::TopEdge, /*proportion*/ 1};
    else if (space.left() - kMinimalTooltipOffset >= labelSize.width() && hasVerticalSpace(space))
        return {Qt::LeftEdge, /*proportion*/ 1};
    else if (space.right() - kMinimalTooltipOffset >= labelSize.width() && hasVerticalSpace(space))
        return {Qt::RightEdge, /*proportion*/ 1};

    BestSide result;
    auto checkSide =
        [&result](Qt::Edge side, qreal labelSize, qreal space)
        {
            const qreal proportion = space / labelSize;
            if (proportion >= 1.0 || proportion >= result.proportion)
            {
                result.proportion = proportion;
                result.edge = side;
            }
        };

    checkSide(Qt::LeftEdge, labelSize.width(), space.left());
    checkSide(Qt::RightEdge, labelSize.width(), space.right());
    checkSide(Qt::TopEdge, labelSize.height(), space.top());
    checkSide(Qt::BottomEdge, labelSize.height(), space.bottom());
    return result;
}

QRectF calculateLabelGeometry(
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

    BestSide bestSide = findBestSide(labelSize, space);

    static constexpr qreal kLabelPadding = 2.0;
    QRectF availableRect = Geometry::dilated(boundingRect.intersected(objectRect), kLabelPadding);

    if (bestSide.proportion < 1.0
        && availableRect.width() / labelSize.width() > bestSide.proportion
        && availableRect.height() / labelSize.height() > bestSide.proportion)
    {
        labelSize = Geometry::bounded(labelSize, availableRect.size());
        return Geometry::aligned(labelSize, availableRect, Qt::AlignTop | Qt::AlignRight);
    }

    static constexpr qreal kArrowBase = 6;
    QPointF pos;
    switch (bestSide.edge)
    {
        case Qt::LeftEdge:
            pos = QPointF(
                objectRect.left() - labelSize.width(),
                objectRect.top() - labelMargins.top() - kArrowBase);
            availableRect = QRectF(
                boundingRect.left(), boundingRect.top(),
                space.left(), boundingRect.height());
            break;
        case Qt::RightEdge:
            pos = QPointF(
                objectRect.right(),
                objectRect.top() - labelMargins.top() - kArrowBase);
            availableRect = QRectF(
                objectRect.right(), boundingRect.top(),
                space.right(), boundingRect.height());
            break;
        case Qt::TopEdge:
            pos = QPointF(
                objectRect.left() - labelMargins.left() - kArrowBase,
                objectRect.top() - labelSize.height());
            availableRect = QRectF(
                boundingRect.left(), boundingRect.top(),
                boundingRect.width(), space.top());
            break;
        case Qt::BottomEdge:
            pos = QPointF(
                objectRect.left() - labelMargins.left() - kArrowBase,
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
    result.name.setPixelSize(fontConfig()->small().pixelSize());

    result.value = baseFont;
    result.value.setPixelSize(fontConfig()->small().pixelSize());
    result.value.setWeight(QFont::Medium);

    result.title = baseFont;
    result.title.setPixelSize(fontConfig()->normal().pixelSize());
    result.title.setWeight(QFont::Medium);

    return result;
}

using FigureItemPtr = QSharedPointer<FigureItem>;
using FigureItemWeakPtr = QWeakPointer<FigureItem>;
using TooltipItemPtr = QSharedPointer<AreaTooltipItem>;

struct Area
{
    AnalyticsOverlayWidget::AreaInfo info;
    FigureItemPtr figureItem;
    TooltipItemPtr tooltipItem;
};

} // namespace

namespace nx::vms::client::desktop {

struct AnalyticsOverlayWidget::Private: public QObject
{
    AnalyticsOverlayWidget* const q = nullptr;

public:
    Private(
        AnalyticsOverlayWidget* q);

    void resetFigureItem(Area& area);

    void setHighlightedArea(const QnUuid& areaId);
    void updateArea(Area& area);
    void updateArea(const QnUuid& areaId);
    void updateAreas();
    void ensureRotation();
    void handleContentUpdated();

    QHash<QnUuid, Area> areaById;
    QnUuid highlightedAreaId;
    bool highlightAreasOnHover = true;
    QPointer<QGraphicsRotation> rotation;
    int angle = 0;
    QRectF zoomRect;

    const figure::RendererPtr renderer;
};

AnalyticsOverlayWidget::Private::Private(AnalyticsOverlayWidget* q):
    q(q),
    renderer(new figure::Renderer(kAreaLineWidth, kNoFillForObjects))
{
}

void AnalyticsOverlayWidget::Private::handleContentUpdated()
{
    // For implementing a correct widget licecycle the following code should be called here:
    //   q->update();
    // However this widget is only used on main scene which is redrawn at fixed rate.
    // Calls to update() are useless here and lead to increased CPU/GPU consumption.
}

void AnalyticsOverlayWidget::Private::setHighlightedArea(const QnUuid& areaId)
{
    if (areaId == highlightedAreaId)
        return;

    const auto oldHighlightedAreaId = highlightedAreaId;
    highlightedAreaId = areaId;
    emit q->highlightedAreaChanged(areaId);

    updateArea(oldHighlightedAreaId);
    updateArea(areaId);

    handleContentUpdated();
}

void AnalyticsOverlayWidget::Private::updateArea(Area& area)
{
    if (!area.figureItem || !area.tooltipItem)
        return;

    const auto figure = area.figureItem->figure();
    if (!figure)
        return;

    figure->setSceneRotation(Rotation::standardRotation(angle));

    const auto pos = figure->pos(q->size());
    area.figureItem->setPos(pos);

    const bool highlighted = highlightedAreaId == area.info.id;
    area.figureItem->setFlag(QGraphicsItem::ItemStacksBehindParent, !highlighted);
    area.tooltipItem->setFlag(QGraphicsItem::ItemStacksBehindParent, !highlighted);
    area.tooltipItem->setText(highlighted ? area.info.hoverText : area.info.text);
    const QRectF& objectRect = figure->boundingRect(q->size());
    const auto& naturalTooltipSize = area.tooltipItem->boundingRect().size();

    const auto& tooltipGeometry = calculateLabelGeometry(
        q->rect(), naturalTooltipSize, area.tooltipItem->textMargins(), objectRect);
    area.tooltipItem->setPos(tooltipGeometry.topLeft());
    const auto scaleFactor =
        std::min(1.0, Geometry::scaleFactor(naturalTooltipSize, tooltipGeometry.size()));
    // Qt does not check that scale factor is different from the previous one.
    // Rescaling is slow on macOS.
    if (!qFuzzyEquals(area.tooltipItem->scale(), scaleFactor))
        area.tooltipItem->setScale(scaleFactor);

    const bool useFigureBacgrkoundColor =
        Geometry::eroded(objectRect, 0.5).contains(tooltipGeometry.center()) || !highlighted;
    area.tooltipItem->setBackgroundColor(useFigureBacgrkoundColor
        ? calculateTooltipColor(figure->color())
        : QColor());
}

void AnalyticsOverlayWidget::Private::updateArea(const QnUuid& areaId)
{
    if (areaId.isNull())
        return;

    const auto it = areaById.find(areaId);
    if (it == areaById.end())
        return;

    updateArea(*it);
}

void AnalyticsOverlayWidget::Private::updateAreas()
{
    for (auto& area: areaById)
        updateArea(area);
}

void AnalyticsOverlayWidget::Private::ensureRotation()
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

AnalyticsOverlayWidget::AnalyticsOverlayWidget(
    QGraphicsWidget* parent)
    :
    base_type(parent),
    d(new Private(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFocusPolicy(Qt::NoFocus);

    connect(this, &QGraphicsWidget::geometryChanged, d.data(), &Private::updateAreas);
}

AnalyticsOverlayWidget::~AnalyticsOverlayWidget()
{
}

void AnalyticsOverlayWidget::addOrUpdateArea(
    const AreaInfo& info,
    const figure::FigurePtr& figure)
{
    if (!NX_ASSERT(!info.id.isNull(), "Area ID can't be empty"))
        return;

    d->ensureRotation();

    auto& area = d->areaById[info.id];
    area.info = info;
    if (!figure)
    {
        // No figure exists yet.
        area.figureItem.reset();
    }
    else if (!area.figureItem)
    {
        // No current figure item created yet.
        area.figureItem.reset(new FigureItem(figure, d->renderer, this));
        connect(area.figureItem.data(), &FigureItem::containsMouseChanged, this,
            [this, areaId = info.id, item = area.figureItem.data()]()
            {
                if (!d->highlightAreasOnHover)
                    return;

                if (item->containsMouse())
                    d->setHighlightedArea(areaId);
                else if (d->highlightedAreaId == areaId)
                    d->setHighlightedArea(QnUuid());
            });

        connect(area.figureItem.data(), &FigureItem::clicked, this,
            [this, areaId = info.id]()
            {
                emit areaClicked(areaId);
            });

        if (area.tooltipItem)
            area.figureItem->stackBefore(area.tooltipItem.data());
    }
    else if (*area.figureItem->figure() != *figure)
    {
        // Just update figure for the item.
        area.figureItem->setFigure(figure);
    }

    auto& tooltipItem = area.tooltipItem;
    if (!tooltipItem)
    {
        tooltipItem.reset(new AreaTooltipItem(this));
        tooltipItem->setTextColor(colorTheme()->color("light1"));
        tooltipItem->setFonts(makeFonts(font()));
        if (area.figureItem)
            area.figureItem->stackBefore(tooltipItem.data());
    }
    tooltipItem->setFigure(figure);

    d->updateArea(area);
    d->handleContentUpdated();
}

void AnalyticsOverlayWidget::removeArea(const QnUuid& areaId)
{
    if (d->areaById.remove(areaId))
    {
        if (d->highlightedAreaId == areaId)
            d->setHighlightedArea(QnUuid());
        NX_VERBOSE(this, "Area was removed, total %1 left", d->areaById.size());
        d->handleContentUpdated();
    }
}

void AnalyticsOverlayWidget::setZoomRect(const QRectF& value)
{
    if (d->zoomRect == value)
        return;

    d->zoomRect = value;
    for (const auto& area: d->areaById)
    {
        if (!area.figureItem)
            continue;

        if (const auto& figure = area.figureItem->figure())
            figure->setSceneRect(d->zoomRect);
    }
}

void AnalyticsOverlayWidget::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* /*option*/,
    QWidget* /*widget*/)
{
    const auto dimmerColor = colorTheme()->color("dark1", 127);

    const auto it = d->areaById.find(d->highlightedAreaId);
    if (it == d->areaById.end())
        return;

    const auto figure = it->figureItem
        ? it->figureItem->figure()
        : figure::FigurePtr();
    if (!figure)
        return;

    QPainterPath figurePath;

    // That's Ok if there are less than 3 points, since we just fading away all other areas
    // and highligting tooltip and non-polygon representation only. For real polygons we also
    // highlight their internal area.
    const auto figurePoints = figure->points(size());
    const auto& figurePolygon = QPolygonF(figurePoints).translated(it->figureItem->pos());
    figurePath.addPolygon(figurePolygon);

    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    QnScopedPainterBrushRollback brushRollback(painter, dimmerColor);

    QPainterPath shadePath;
    shadePath.addRect(geometry());

    painter->drawPath(shadePath.subtracted(figurePath));
}

bool AnalyticsOverlayWidget::event(QEvent* event)
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
