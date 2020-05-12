#include "roi_figures_overlay_widget.h"

#include <cmath>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QPainter>
#include <QtWidgets/QOpenGLWidget>

#include <client/client_module.h>

#include <nx/fusion/model_functions.h>
#include <nx/client/core/utils/geometry.h>
#include <nx/vms/client/core/common/utils/path_util.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_multi_listener.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workaround/gl_native_painting.h>

#include <opengl_renderer.h>
#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/shaders/color_line_shader_program.h>


namespace nx::vms::client::desktop {

using core::Geometry;

namespace {

static constexpr qreal kDirectionMarkWidth = 16;
static constexpr qreal kDirectionMarkHeight = 8;
static constexpr qreal kDirectionMarkOffset = 3;

static const QVector<QPointF> kDirectionMark{
    {kDirectionMarkOffset, -kDirectionMarkWidth / 2},
    {kDirectionMarkOffset + kDirectionMarkHeight, 0},
    {kDirectionMarkOffset, kDirectionMarkWidth / 2}
};

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

    item.visible = object.value(QStringLiteral("showOnCamera")).toBool();
    item.color = figure.value(QStringLiteral("color")).toString(
        ColorTheme::instance()->color("roi1").name());

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
    if (direction == QStringLiteral("left"))
        line.direction = Line::Direction::left;
    else if (direction == QStringLiteral("right"))
        line.direction = Line::Direction::right;

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

    QOpenGLVertexArrayObject m_lineVertices;
    QOpenGLBuffer m_lineBuffer;
    bool m_lineVaoInitialized = false;

    QOpenGLVertexArrayObject m_triangleVertices;
    QOpenGLBuffer m_trianglePosBuffer;
    bool m_triangleVaoInitialized = false;

public:
    qreal lineWidth = 2;
    qreal regionOpacity = 0.15;

    QMap<QString, Line> lines;
    QMap<QString, Box> boxes;
    QMap<QString, Polygon> polygons;

    AnalyticsSettingsMultiListener* settingsListener = nullptr;
    QHash<QnUuid, QHash<QString, FigureType>> figureKeysByEngineId;

public:
    Private(RoiFiguresOverlayWidget* q);

    QPointF absolutePos(const QPointF& relativePos) const;
    QVector<QPointF> mapPoints(const QVector<QPointF>& points) const;

    void strokePolyline(
        QPainter* painter,
        QWidget* widget,
        const QVector<QPointF>& points,
        const QColor& color,
        bool closed,
        qreal lineWidth,
        bool beginNativePainting = true);
    void drawLine(QPainter* painter, const Line& line, QWidget* widget);
    void drawBox(QPainter* painter, const Box& box, QWidget* widget);
    void drawPolygon(QPainter* painter, const Polygon& polygon, QWidget* widget);
    void drawDirectionMark(QPainter* painter,
        const QPointF& position,
        qreal angle,
        const QColor& color,
        QWidget* widget);

    void updateFigureKeys(const QnUuid& engineId, const DeviceAgentData& data);
    void updateFigures();

    void ensureLineVao(QnGLShaderProgram* shader);
    void ensureTriangleVao(QnGLShaderProgram* shader);
};

RoiFiguresOverlayWidget::Private::Private(RoiFiguresOverlayWidget* q):
    q(q)
{
}

void RoiFiguresOverlayWidget::Private::ensureTriangleVao(QnGLShaderProgram* shader)
{
    if (m_triangleVaoInitialized)
        return;

    m_triangleVertices.create();
    m_triangleVertices.bind();

    m_trianglePosBuffer.create();
    m_trianglePosBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_trianglePosBuffer.bind();

    static constexpr auto kPositionAttribute = "aPosition";
    const auto posLocation = shader->attributeLocation(kPositionAttribute);
    NX_ASSERT(posLocation != -1, kPositionAttribute);

    std::vector<GLdouble> vertices;
    for (const auto& point: kDirectionMark)
    {
        vertices.push_back(point.x());
        vertices.push_back(point.y());
    }
    m_trianglePosBuffer.allocate(vertices.data(), sizeof(GLdouble) * vertices.size());

    shader->enableAttributeArray(posLocation);
    shader->setAttributeBuffer(posLocation, GL_DOUBLE, 0, 2);

    m_trianglePosBuffer.release();

    m_triangleVertices.release();

    m_triangleVaoInitialized = true;
}

void RoiFiguresOverlayWidget::Private::ensureLineVao(QnGLShaderProgram* shader)
{
    if (m_lineVaoInitialized)
        return;

    m_lineVertices.create();
    m_lineVertices.bind();

    m_lineBuffer.create();
    m_lineBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_lineBuffer.bind();

    static constexpr auto kDataAttribute = "vData";
    const auto location = shader->attributeLocation(kDataAttribute);
    NX_ASSERT(location != -1, kDataAttribute);

    shader->enableAttributeArray(location);
    shader->setAttributeBuffer(
        location, GL_DOUBLE, 0, QnColorLineGLShaderProgram::kComponentPerVertex);

    m_lineBuffer.release();

    m_lineVertices.release();

    m_lineVaoInitialized = true;
}

QPointF RoiFiguresOverlayWidget::Private::absolutePos(const QPointF& relativePos) const
{
    const QRectF widgetRect({}, q->size());
    return Geometry::bounded(Geometry::subPoint(widgetRect, relativePos), widgetRect);
}

QVector<QPointF> RoiFiguresOverlayWidget::Private::mapPoints(const QVector<QPointF>& points) const
{
    QVector<QPointF> result;
    for (const auto& point: points)
        result.append(absolutePos(point));
    return result;
}

void RoiFiguresOverlayWidget::Private::strokePolyline(
    QPainter* painter,
    QWidget* widget,
    const QVector<QPointF>& points,
    const QColor& color,
    bool closed,
    qreal lineWidth,
    bool beginNativePainting)
{
    const auto glWidget = qobject_cast<QOpenGLWidget*>(widget);
    const auto functions = QOpenGLContext::currentContext()->functions();

    if (beginNativePainting)
    {
        QnGlNativePainting::begin(glWidget, painter);

        functions->glEnable(GL_BLEND);
        functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Make the line slightly wider for better antialiasing.
    constexpr auto kLineScale = 1.2;
    constexpr auto kLineFeather = 0.3;

    auto renderer = QnOpenGLRendererManager::instance(glWidget);
    auto shader = renderer->getColorLineShader();
    ensureLineVao(shader);

    const std::vector<GLdouble> triangles = QnColorLineGLShaderProgram::genVericesFromLineStrip(
        points, closed, lineWidth * kLineScale);
    m_lineBuffer.bind();
    m_lineBuffer.allocate(triangles.data(), sizeof(GLdouble) * triangles.size());
    m_lineBuffer.release();

    shader->bind();
    shader->setModelViewProjectionMatrix(
        renderer->getProjectionMatrix() * renderer->getModelViewMatrix());
    shader->setColor(QVector4D(color.redF(), color.greenF(), color.blueF(), painter->opacity()));
    shader->setFeather(kLineFeather);

    m_lineVertices.bind();
    functions->glDrawArrays(
        GL_TRIANGLES, 0, triangles.size() / QnColorLineGLShaderProgram::kComponentPerVertex);
    m_lineVertices.release();

    shader->release();

    if (beginNativePainting)
    {
        functions->glDisable(GL_BLEND);
        functions->glDisable(GL_LINE_SMOOTH);

        QnGlNativePainting::end(painter);
    }
}

void RoiFiguresOverlayWidget::Private::drawLine(
    QPainter* painter, const Line& line, QWidget* widget)
{
    if (line.points.size() < 2 || !line.visible)
        return;

    const auto& points = mapPoints(line.points);

    strokePolyline(
        painter,
        widget,
        points,
        line.color,
        false,
        lineWidth);

    core::PathUtil pathUtil;
    pathUtil.setPoints(points);

    if (line.direction == Line::Direction::left || line.direction == Line::Direction::none)
    {
        drawDirectionMark(
            painter,
            pathUtil.midAnchorPoint(),
            pathUtil.midAnchorPointNormalAngle(),
            line.color,
            widget);
    }

    if (line.direction == Line::Direction::right || line.direction == Line::Direction::none)
    {
        drawDirectionMark(
            painter,
            pathUtil.midAnchorPoint(),
            pathUtil.midAnchorPointNormalAngle() + M_PI,
            line.color,
            widget);
    }
}

void RoiFiguresOverlayWidget::Private::drawBox(QPainter* painter, const Box& box, QWidget* widget)
{
    if (box.points.size() != 2 || !box.visible)
        return;

    QRectF rect(absolutePos(box.points.first()), absolutePos(box.points.last()));
    QVector<QPointF> points{
        rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};

    QColor fillColor = box.color;
    fillColor.setAlphaF(regionOpacity);
    painter->fillRect(rect, fillColor);
    strokePolyline(
        painter,
        widget,
        points,
        box.color,
        true,
        lineWidth);
}

void RoiFiguresOverlayWidget::Private::drawPolygon(
    QPainter* painter, const Polygon& polygon, QWidget* widget)
{
    if (polygon.points.empty() || !polygon.visible)
        return;

    const auto& points = mapPoints(polygon.points);

    QPainterPath path;
    path.addPolygon(points);
    QColor fillColor = polygon.color;
    fillColor.setAlphaF(regionOpacity);
    painter->fillPath(path, fillColor);

    strokePolyline(
        painter,
        widget,
        points,
        polygon.color,
        true,
        lineWidth);
}

void RoiFiguresOverlayWidget::Private::drawDirectionMark(
    QPainter* painter, const QPointF& position, qreal angle, const QColor& color, QWidget* widget)
{
    const auto glWidget = qobject_cast<QOpenGLWidget*>(widget);

    QnGlNativePainting::begin(glWidget, painter);

    const auto functions = QOpenGLContext::currentContext()->functions();

    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto renderer = QnOpenGLRendererManager::instance(glWidget);

    auto modelViewMatrix = renderer->getModelViewMatrix();

    modelViewMatrix.translate(position.x(), position.y());
    modelViewMatrix.rotate(angle / M_PI * 180, 0.0, 0.0, 1.0);

    renderer->pushModelViewMatrix();
    renderer->setModelViewMatrix(modelViewMatrix);

    auto shader = renderer->getColorShader();
    ensureTriangleVao(shader);

    shader->bind();
    shader->setModelViewProjectionMatrix(renderer->getProjectionMatrix() * renderer->getModelViewMatrix());
    shader->setColor(QVector4D(color.redF(), color.greenF(), color.blueF(), painter->opacity()));

    m_triangleVertices.bind();
    functions->glDrawArrays(GL_TRIANGLES, 0, kDirectionMark.size());
    m_triangleVertices.release();

    shader->release();

    strokePolyline(painter, widget, kDirectionMark, color, true, 1, false);

    renderer->popModelViewMatrix();

    functions->glDisable(GL_BLEND);

    QnGlNativePainting::end(painter);
}

void RoiFiguresOverlayWidget::Private::updateFigureKeys(
    const QnUuid& engineId, const DeviceAgentData& data)
{
    figureKeysByEngineId.insert(engineId, findFigureKeys(data.model));
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

        const QJsonObject& values = settingsListener->data(engineId).values;

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
        d->settingsListener = new AnalyticsSettingsMultiListener(
            qnClientModule->analyticsSettingsManager(),
            camera,
            AnalyticsSettingsMultiListener::ListenPolicy::enabledEngines,
            this);

        connect(d->settingsListener, &AnalyticsSettingsMultiListener::dataChanged, this,
            [this](const QnUuid& engineId, const DeviceAgentData& data)
            {
                d->updateFigureKeys(engineId, data);
                d->updateFigures();
            });

        auto initializeKeys =
            [this]()
            {
                d->figureKeysByEngineId.clear();
                for (const QnUuid& engineId: d->settingsListener->engineIds())
                    d->updateFigureKeys(engineId, d->settingsListener->data(engineId));
                d->updateFigures();
            };
        connect(
            d->settingsListener, &AnalyticsSettingsMultiListener::enginesChanged,
            this, initializeKeys);
        initializeKeys();
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
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* widget)
{
    painter->save();

    for (const auto& polygon: d->polygons)
        d->drawPolygon(painter, polygon, widget);

    for (const auto& box: d->boxes)
        d->drawBox(painter, box, widget);

    for (const auto& line: d->lines)
        d->drawLine(painter, line, widget);

    painter->restore();
}

} // namespace nx::vms::client::desktop
