#include "roi_visualization.h"

#include <QtCore/QVariantMap>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QTransform>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/vms/api/analytics/settings_response.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/random.h>
#include <nx/utils/range_adapters.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <utils/math/color_transformations.h>

#include "data/drawing_items.h"
#include <utils/common/scoped_painter_rollback.h>

namespace nx::vms::client::desktop::integrations::internal {

namespace {

static const QString kPolygonKey("nx.sys.drawing.polygon.");
static const QString kLinesKey("nx.sys.drawing.lines.");
static const QString kLineKey("nx.sys.drawing.line.");
static const QString kBoxKey("nx.sys.drawing.box.");

static const std::vector<QColor> kPalette{
    Qt::GlobalColor::black,
    Qt::GlobalColor::white,
    Qt::GlobalColor::darkGray,
    Qt::GlobalColor::gray,
    Qt::GlobalColor::lightGray,
    Qt::GlobalColor::red,
    Qt::GlobalColor::green,
    Qt::GlobalColor::blue,
    Qt::GlobalColor::cyan,
    Qt::GlobalColor::magenta,
    Qt::GlobalColor::yellow,
    Qt::GlobalColor::darkRed,
    Qt::GlobalColor::darkGreen,
    Qt::GlobalColor::darkBlue,
    Qt::GlobalColor::darkCyan,
    Qt::GlobalColor::darkMagenta,
    Qt::GlobalColor::darkYellow,
};

static constexpr qreal kRegionTransparency = 0.15;
static constexpr qreal kPointRadius = 1.5;
static constexpr qreal kDirectionMarkWidth = 16;
static constexpr qreal kDirectionMarkHeight = 8;
static constexpr qreal kDirectionMarkOffset = 2;

static const QPolygonF kMark(
    {
        {kDirectionMarkOffset, -kDirectionMarkWidth / 2},
        {kDirectionMarkOffset + kDirectionMarkHeight, 0},
        {kDirectionMarkOffset, kDirectionMarkWidth / 2}
    });

QColor randomColor()
{
    return nx::utils::random::choice(kPalette);
}

template<typename T>
void fixColor(T& item)
{
    if (!item.color.isValid())
        item.color = randomColor();
}

template<typename T>
void processItem(T&& item, std::vector<T>& storage)
{
    if (item.isValid())
    {
        fixColor(item);
        storage.push_back(item);
    }
    else
    {
        NX_WARNING(typeid(RoiVisualizationIntegration), "Item is invalid");
    }
}

QPointF toPointF(const roi::Point& point, const QRectF& rect)
{
    return core::Geometry::subPoint(rect, QPointF(point[0], point[1]));
}

QVector<QPointF> toPoints(const roi::Points& points, const QRectF& rect)
{
    QVector<QPointF> result;
    std::transform(points.cbegin(), points.cend(), std::back_inserter(result),
        [&rect](const roi::Point& point) { return toPointF(point, rect); });
    return result;
}

void paintRegion(const QColor& color, const QPainterPath& path, QPainter* painter)
{
    painter->fillPath(path, toTransparent(color, kRegionTransparency));

    QPen pen;
    pen.setCosmetic(true);
    pen.setBrush(color);
    painter->strokePath(path, pen);
}

void paintPoints(const QColor& color, const QVector<QPointF>& points, QPainter* painter)
{
    QPainterPath path;
    for (const auto& point: points)
        path.addEllipse(point, kPointRadius, kPointRadius);
    painter->fillPath(path, color);
}

void paintPolygon(const roi::Polygon& polygon, const QRectF& rect, QPainter* painter)
{
    const auto points = toPoints(polygon.points, rect);

    QPainterPath path;
    path.addPolygon(points);
    path.closeSubpath();
    paintRegion(polygon.color, path, painter);
    paintPoints(polygon.color, points, painter);
}

void paintBox(const roi::Box& box, const QRectF& rect, QPainter* painter)
{
    const auto points = toPoints(box.points, rect);

    QPainterPath path;
    path.addRect(QRectF(points[0], points[1]));
    path.closeSubpath();
    paintRegion(box.color, path, painter);
    paintPoints(box.color, points, painter);
}

// Paint direction mark to the left of the line.
void paintDirectionMark(const QColor& color, const QLineF& line, QPainter* painter)
{
    auto transform = QTransform::fromTranslate(-line.center().x(), -line.center().y());
    transform *= QTransform().rotateRadians(std::atan2(line.dx(), line.dy()));

    if (!NX_ASSERT(transform.isInvertible()))
        return;

    const QTransform inverted = transform.inverted();

    QPainterPath path;
    path.addPolygon(inverted.map(kMark));
    path.closeSubpath();
    painter->fillPath(path, color);
}

void paintLine(const roi::Line& line, const QRectF& rect, QPainter* painter)
{
    const auto points = toPoints(line.points, rect);

    QPainterPath path;
    path.moveTo(points[0]);
    path.lineTo(points[1]);

    QPen pen;
    pen.setCosmetic(true);
    pen.setBrush(line.color);
    painter->strokePath(path, pen);

    paintPoints(line.color, points, painter);
    if (line.direction == roi::Direction::left || line.direction == roi::Direction::both)
        paintDirectionMark(line.color, QLineF(points[0], points[1]), painter);
    if (line.direction == roi::Direction::right || line.direction == roi::Direction::both)
        paintDirectionMark(line.color, QLineF(points[1], points[0]), painter);
}

} // namespace

struct RoiVisualizationIntegration::Private
{
    struct DrawingItems
    {
        std::vector<roi::Box> boxes;
        std::vector<roi::Polygon> polygons;
        std::vector<roi::Line> lines;
    };

    QHash<QnMediaResourceWidget*, DrawingItems> cache;

    void processValues(QnMediaResourceWidget* widget, const QVariantMap& values)
    {
        auto& drawingItems = cache[widget];
        for (auto [key, variant]: nx::utils::constKeyValueRange(values))
        {
            QByteArray value = variant.toString().toUtf8();
            if (key.startsWith(kBoxKey))
            {
                auto item = QJson::deserialized<roi::Box>(value);
                processItem(std::move(item), drawingItems.boxes);
            }
            else if (key.startsWith(kPolygonKey))
            {
                auto item = QJson::deserialized<roi::Polygon>(value);
                processItem(std::move(item), drawingItems.polygons);
            }
            else if (key.startsWith(kLineKey))
            {
                auto item = QJson::deserialized<roi::Line>(value);
                processItem(std::move(item), drawingItems.lines);
            }
            else if (key.startsWith(kLinesKey))
            {
                const auto lines = QJson::deserialized<roi::Lines>(value);
                for (auto line: lines)
                    processItem(std::move(line), drawingItems.lines);
            }
        }
    }
};

RoiVisualizationIntegration::RoiVisualizationIntegration(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

RoiVisualizationIntegration::~RoiVisualizationIntegration()
{
}

void RoiVisualizationIntegration::registerWidget(QnMediaResourceWidget* widget)
{
    d->cache.insert(widget, {});

    const auto camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    const auto server = camera->getParentServer();
    if (!NX_ASSERT(server))
        return;

    const auto engines = camera->enabledAnalyticsEngineResources();
    for (const auto& engine: engines)
    {
        const auto handle = server->restConnection()->getDeviceAnalyticsSettings(
            camera,
            engine,
            nx::utils::guarded(this,
                [this, widget = QPointer<QnMediaResourceWidget>(widget)](
                    bool success,
                    rest::Handle requestId,
                    const nx::vms::api::analytics::SettingsResponse& result)
                {
                    if (success && widget && d->cache.contains(widget))
                        d->processValues(widget, result.values.toVariantMap());
                }),
            thread());
    }
}

void RoiVisualizationIntegration::unregisterWidget(QnMediaResourceWidget* widget)
{
    d->cache.remove(widget);
}

void RoiVisualizationIntegration::paintVideoOverlay(
    QnMediaResourceWidget* widget,
    QPainter* painter)
{
    if (!NX_ASSERT(d->cache.contains(widget)))
        return;

    const PainterTransformScaleStripper scaleStripper(painter);
    const auto rect = scaleStripper.mapRect(widget->rect());

    QnScopedPainterAntialiasingRollback antialiasing(painter, true);

    const auto& drawingItems = d->cache.value(widget);

    for (const auto& polygon: drawingItems.polygons)
        paintPolygon(polygon, rect, painter);

    for (const auto& box: drawingItems.boxes)
        paintBox(box, rect, painter);

    for (const auto& line: drawingItems.lines)
        paintLine(line, rect, painter);
}

} // namespace nx::vms::client::desktop::integrations::internal
