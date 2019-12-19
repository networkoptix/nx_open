#include "roi_figures_overlay_widget.h"

#include <cmath>

#include <QtGui/QPainter>

#include <nx/fusion/model_functions.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/vms/client/core/common/utils/path_util.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_multi_listener.h>

#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/resource_widget.h>

namespace nx::vms::client::desktop {

using core::Geometry;

namespace {

static constexpr qreal kDirectionMarkWidth = 16;
static constexpr qreal kDirectionMarkHeight = 8;
static constexpr qreal kDirectionMarkOffset = 2;

static const QPolygonF kMark(
    {
        {kDirectionMarkOffset, -kDirectionMarkWidth / 2},
        {kDirectionMarkOffset + kDirectionMarkHeight, 0},
        {kDirectionMarkOffset, kDirectionMarkWidth / 2},
        {kDirectionMarkOffset, -kDirectionMarkWidth / 2}
    });

enum class FigureType
{
    invalid, line, box, polygon
};

QHash<QString, FigureType> findFigureKeys(const QJsonObject& model)
{
    static const QHash<QString, FigureType> kFigureTypes{
        {"LineFigure", FigureType::line},
        {"BoxFigure", FigureType::box},
        {"PolygonFigure", FigureType::polygon},
    };

    const auto type = kFigureTypes.value(
        model.value(QStringLiteral("type")).toString(), FigureType::invalid);

    if (type != FigureType::invalid)
    {
        if (const auto name = model.value(QStringLiteral("name")).toString(); !name.isEmpty())
            return QHash<QString, FigureType>{{name, type}};
        return {};
    }

    QHash<QString, FigureType> result;

    // TODO: Recursive parser is not 100% correct. Protect it against injection of invalid items
    // and sections.
    const auto items = model.value(QStringLiteral("items")).toArray();
    if (!items.isEmpty())
    {
        for (const auto& value: items)
        {
            if (value.isObject())
                result.unite(findFigureKeys(value.toObject()));
        }
    }

    const auto sections = model.value(QStringLiteral("sections")).toArray();
    if (!sections.isEmpty())
    {
        for (const auto& value: sections)
        {
            if (value.isObject())
                result.unite(findFigureKeys(value.toObject()));
        }
    }

    return result;
}

void parseItem(RoiFiguresOverlayWidget::Item& item, const QJsonObject& object)
{
    const QJsonObject& figure = object.value(QStringLiteral("figure")).toObject();
    if (figure.isEmpty())
        return;

    item.visible = object.value(QStringLiteral("showOnCamera")).toBool(true);
    item.color = figure.value(QStringLiteral("color")).toString();

    item.points.clear();
    for (const auto p: figure.value(QStringLiteral("points")).toArray())
    {
        if (!p.isArray())
            continue;

        const auto& coordinates = p.toArray();
        if (coordinates.size() < 2)
            continue;

        item.points.append({coordinates[0].toDouble(), coordinates[1].toDouble()});
    }
}

RoiFiguresOverlayWidget::Line parseLine(const QJsonObject& object)
{
    using Line = RoiFiguresOverlayWidget::Line;

    const QJsonObject& figure = object.value(QStringLiteral("figure")).toObject();
    if (figure.isEmpty())
        return {};

    Line line;
    parseItem(line, object);

    const auto& direction = figure.value(QStringLiteral("direction")).toString();
    if (direction == QStringLiteral("a"))
        line.direction = Line::Direction::a;
    else if (direction == QStringLiteral("b"))
        line.direction = Line::Direction::b;

    return line;
}

RoiFiguresOverlayWidget::Box parseBox(const QJsonObject& object)
{
    RoiFiguresOverlayWidget::Box box;
    parseItem(box, object);
    return box;
}

RoiFiguresOverlayWidget::Polygon parsePolygon(const QJsonObject& object)
{
    RoiFiguresOverlayWidget::Polygon polygon;
    parseItem(polygon, object);
    return polygon;
}

} // namespace

class RoiFiguresOverlayWidget::Private: public QObject
{
    RoiFiguresOverlayWidget* q;

public:
    qreal lineWidth = 2;
    qreal pointRadius = 1.5;
    qreal regionTransparency = 0.15;

    QMap<QString, Line> lines;
    QMap<QString, Box> boxes;
    QMap<QString, Polygon> polygons;

    AnalyticsSettingsMultiListener* settingsListener = nullptr;
    QHash<QnUuid, QHash<QString, FigureType>> figureKeysByEngineId;

public:
    Private(RoiFiguresOverlayWidget* q);

    QPointF absolutePos(const QPointF& relativePos) const;
    QVector<QPointF> mapPoints(const QVector<QPointF>& points) const;

    void setupPainter(QPainter* painter, const Item& item);
    void drawLine(QPainter* painter, const Line& line);
    void drawBox(QPainter* painter, const Box& box);
    void drawPolygon(QPainter* painter, const Polygon& polygon);
    void drawPoints(QPainter* painter, const QVector<QPointF>& points, const QColor& color);
    void drawDirectionMark(
        QPainter* painter, const QPointF& position, qreal angle, const QColor& color);

    void updateFigureKeys(const QnUuid& engineId, const QJsonObject& model);
    void updateFigures();
};

RoiFiguresOverlayWidget::Private::Private(RoiFiguresOverlayWidget* q):
    q(q)
{
}

QPointF RoiFiguresOverlayWidget::Private::absolutePos(const QPointF& relativePos) const
{
    return Geometry::subPoint(QRectF({}, q->size()), relativePos);
}

QVector<QPointF> RoiFiguresOverlayWidget::Private::mapPoints(const QVector<QPointF>& points) const
{
    QVector<QPointF> result;
    for (const auto& point: points)
        result.append(absolutePos(point));
    return result;
}

void RoiFiguresOverlayWidget::Private::setupPainter(QPainter* painter, const Item& item)
{
    painter->setPen(QPen(item.color, lineWidth));
    QColor brushColor = item.color;
    brushColor.setAlphaF(regionTransparency);
    painter->setBrush(brushColor);
}

void RoiFiguresOverlayWidget::Private::drawLine(QPainter* painter, const Line& line)
{
    if (line.points.size() < 2 || !line.visible)
        return;

    setupPainter(painter, line);

    const auto& points = mapPoints(line.points);
    painter->setBrush({});

    QPainterPath path;
    path.moveTo(points.first());
    for (auto it = std::next(points.begin()); it != points.end(); ++it)
        path.lineTo(*it);
    painter->drawPath(path);
    drawPoints(painter, points, line.color);

    core::PathUtil pathUtil;
    pathUtil.setPoints(points);

    if (line.direction == Line::Direction::a || line.direction == Line::Direction::none)
    {
        drawDirectionMark(
            painter,
            pathUtil.midAnchorPoint(),
            pathUtil.midAnchorPointNormalAngle(),
            line.color);
    }
    if (line.direction == Line::Direction::b || line.direction == Line::Direction::none)
    {
        drawDirectionMark(
            painter,
            pathUtil.midAnchorPoint(),
            pathUtil.midAnchorPointNormalAngle() + M_PI,
            line.color);
    }
}

void RoiFiguresOverlayWidget::Private::drawBox(QPainter* painter, const Box& box)
{
    if (box.points.size() != 2 || !box.visible)
        return;

    QRectF rect(absolutePos(box.points.first()), absolutePos(box.points.last()));
    QVector<QPointF> points{
        rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};

    setupPainter(painter, box);
    painter->drawRect(rect);
    drawPoints(painter, points, box.color);
}

void RoiFiguresOverlayWidget::Private::drawPolygon(QPainter* painter, const Polygon& polygon)
{
    if (polygon.points.empty() || !polygon.visible)
        return;

    setupPainter(painter, polygon);

    const auto& points = mapPoints(polygon.points);
    painter->drawPolygon(QPolygonF(points));
    drawPoints(painter, points, polygon.color);
}

void RoiFiguresOverlayWidget::Private::drawPoints(
    QPainter* painter, const QVector<QPointF>& points, const QColor& color)
{
    painter->setPen(QPen(color, 1));
    painter->setBrush(color);
    for (const auto& point: points)
        painter->drawEllipse(point, pointRadius, pointRadius);
}

void RoiFiguresOverlayWidget::Private::drawDirectionMark(
    QPainter* painter, const QPointF& position, qreal angle, const QColor& color)
{
    auto transform = QTransform::fromTranslate(-position.x(), -position.y());
    transform *= QTransform().rotateRadians(-angle);

    if (!NX_ASSERT(transform.isInvertible()))
        return;

    const QTransform inverted = transform.inverted();

    QPainterPath path;
    path.addPolygon(inverted.map(kMark));
    path.closeSubpath();
    painter->fillPath(path, color);
}

void RoiFiguresOverlayWidget::Private::updateFigureKeys(
    const QnUuid& engineId, const QJsonObject& model)
{
    if (!settingsListener)
        return;

    figureKeysByEngineId.insert(engineId, findFigureKeys(model));

    updateFigures();
}

void RoiFiguresOverlayWidget::Private::updateFigures()
{
    lines.clear();
    boxes.clear();
    polygons.clear();

    for (auto it = figureKeysByEngineId.begin(); it != figureKeysByEngineId.end(); ++it)
    {
        const QnUuid& engineId = it.key();
        const auto& keys = it.value();

        if (keys.isEmpty())
            continue;

        const QJsonObject& values = settingsListener->values(engineId);

        for (auto it = keys.begin(); it != keys.end(); ++it)
        {
            const QJsonObject& value = values.value(it.key()).toObject();
            if (value.isEmpty())
                continue;

            switch (it.value())
            {
                case FigureType::line:
                    if (const auto& line = parseLine(value); line.isValid())
                        q->addLine(QUuid::createUuid().toString(), line);
                    break;
                case FigureType::box:
                    if (const auto& box = parseBox(value); box.isValid())
                        q->addBox(QUuid::createUuid().toString(), box);
                    break;
                case FigureType::polygon:
                    if (const auto& polygon = parsePolygon(value); polygon.isValid())
                        q->addPolygon(QUuid::createUuid().toString(), polygon);
                    break;
                default:
                    break;
            }
        }
    }
}

//-------------------------------------------------------------------------------------------------

RoiFiguresOverlayWidget::RoiFiguresOverlayWidget(
    QGraphicsWidget* parent,
    QnResourceWidget* resourceWidget)
    :
    GraphicsWidget(parent),
    d(new Private(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFocusPolicy(Qt::NoFocus);

    if (const auto camera = resourceWidget->resource().dynamicCast<QnVirtualCameraResource>())
    {
        d->settingsListener = new AnalyticsSettingsMultiListener(camera, this);

        connect(d->settingsListener, &AnalyticsSettingsMultiListener::modelChanged, d.data(),
            &Private::updateFigureKeys);
        connect(d->settingsListener, &AnalyticsSettingsMultiListener::valuesChanged, d.data(),
            &Private::updateFigures);

        for (const QnUuid& engineId: d->settingsListener->engineIds())
            d->updateFigureKeys(engineId, d->settingsListener->model(engineId));
    }
}

RoiFiguresOverlayWidget::~RoiFiguresOverlayWidget()
{
}

void RoiFiguresOverlayWidget::addLine(const QString& id, const RoiFiguresOverlayWidget::Line& line)
{
    d->lines.insert(id, line);
    update();
}

void RoiFiguresOverlayWidget::removeLine(const QString& id)
{
    d->lines.remove(id);
    update();
}

void RoiFiguresOverlayWidget::addBox(const QString& id, const RoiFiguresOverlayWidget::Box& box)
{
    d->boxes.insert(id, box);
    update();
}

void RoiFiguresOverlayWidget::removeBox(const QString& id)
{
    d->boxes.remove(id);
    update();
}

void RoiFiguresOverlayWidget::addPolygon(
    const QString& id, const RoiFiguresOverlayWidget::Polygon& polygon)
{
    d->polygons.insert(id, polygon);
    update();
}

void RoiFiguresOverlayWidget::removePolygon(const QString& id)
{
    d->polygons.remove(id);
    update();
}

void RoiFiguresOverlayWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    painter->save();

    for (const auto& polygon: d->polygons)
        d->drawPolygon(painter, polygon);

    for (const auto& box: d->boxes)
        d->drawBox(painter, box);

    for (const auto& line: d->lines)
        d->drawLine(painter, line);

    painter->restore();
}

} // namespace nx::vms::client::desktop
