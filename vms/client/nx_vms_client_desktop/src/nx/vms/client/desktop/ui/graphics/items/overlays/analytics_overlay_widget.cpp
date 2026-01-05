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
#include <nx/vms/common/utils/object_painter_helper.h>
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

    void setHighlightedArea(const nx::Uuid& areaId);
    void updateArea(Area& area);
    void updateArea(const nx::Uuid& areaId);
    void updateAreas();
    void ensureRotation();
    void handleContentUpdated();

    QHash<nx::Uuid, Area> areaById;
    nx::Uuid highlightedAreaId;
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
    // For implementing a correct widget lifecycle the following code should be called here:
    //   q->update();
    // However this widget is only used on main scene which is redrawn at fixed rate.
    // Calls to update() are useless here and lead to increased CPU/GPU consumption.
}

void AnalyticsOverlayWidget::Private::setHighlightedArea(const nx::Uuid& areaId)
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

    const auto& tooltipGeometry = common::ObjectPainterHelper::calculateLabelGeometry(
        q->rect(),
        naturalTooltipSize,
        area.tooltipItem->textMargins(),
        objectRect);
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
        ? common::ObjectPainterHelper::calculateTooltipColor(figure->color())
        : QColor());
}

void AnalyticsOverlayWidget::Private::updateArea(const nx::Uuid& areaId)
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
                    d->setHighlightedArea(nx::Uuid());
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

void AnalyticsOverlayWidget::removeArea(const nx::Uuid& areaId)
{
    if (d->areaById.remove(areaId))
    {
        if (d->highlightedAreaId == areaId)
            d->setHighlightedArea(nx::Uuid());
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
    // and highlighting tooltip and non-polygon representation only. For real polygons we also
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
