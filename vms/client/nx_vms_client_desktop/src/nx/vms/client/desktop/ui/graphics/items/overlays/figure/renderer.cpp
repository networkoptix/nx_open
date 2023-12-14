// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "renderer.h"

#include <cmath>

#include <QtCore/qmath.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QPainter>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/common/utils/path_util.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/opengl/opengl_renderer.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/figure/decorations_helper.h>
#include <ui/graphics/shaders/base_shader_program.h>
#include <ui/graphics/shaders/color_line_shader_program.h>
#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/workaround/gl_native_painting.h>

#include "figure.h"
#include "open_shape_figure.h"
#include "polyline.h"

namespace {

using nx::vms::client::desktop::figure::Points;
using nx::vms::client::desktop::figure::Polyline;

enum class DrawBatchMode
{
    singleOperation,
    batchOperation
};

enum class FigureShapeType
{
    open,
    closed
};

static const auto kDirectionMark =
    []() -> Points
    {
        return {
            {Polyline::kDirectionMarkOffset, -Polyline::kDirectionMarkWidth / 2},
            {Polyline::kDirectionMarkOffset + Polyline::kDirectionMarkHeight, 0},
            {Polyline::kDirectionMarkOffset, Polyline::kDirectionMarkWidth / 2}};
    }();

} // namespace

//-------------------------------------------------------------------------------------------------

namespace nx::vms::client::desktop::figure {

struct Renderer::Private
{
    const int lineWidth;
    const bool fillClosedShapes;
    QOpenGLVertexArrayObject lineVertices;
    QOpenGLBuffer lineBuffer;
    bool lineVaoInitialized = false;

    QOpenGLVertexArrayObject triangleVertices;
    QOpenGLBuffer trianglePosBuffer;
    bool triangleVaoInitialized = false;

    Private(
        int lineWidth,
        bool fillClosedShapes);
    void ensureLineVao(QnGLShaderProgram* shader);
    void ensureTriangleVao(QnGLShaderProgram* shader);

    void drawPolyline(
        QPainter* painter,
        QWidget* widget,
        const Points& points,
        const QColor& color,
        int width,
        FigureShapeType type,
        DrawBatchMode mode = DrawBatchMode::singleOperation);

    void drawDirectionMark(
        QPainter* painter,
        QWidget* widget,
        const QPointF& position,
        qreal angle,
        const QColor& color);

    void drawLineMarks(
        QPainter* painter,
        QWidget* widget,
        const Points::value_type& startPoint,
        const Points::value_type& endPoint,
        const QColor& color,
        Polyline::Direction direction);

    void drawCross(
        QPainter* painter,
        QWidget* widget,
        const QRectF& boundingRect,
        const QColor& color);
};

Renderer::Private::Private(
    int lineWidth,
    bool fillClosedShapes)
    :
    lineWidth(lineWidth),
    fillClosedShapes(fillClosedShapes)
{
}

void Renderer::Private::ensureLineVao(QnGLShaderProgram* shader)
{
    if (lineVaoInitialized)
        return;

    lineVertices.create();
    lineVertices.bind();

    lineBuffer.create();
    lineBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    lineBuffer.bind();

    static constexpr auto kDataAttribute = "vData";
    const auto location = shader->attributeLocation(kDataAttribute);
    NX_ASSERT(location != -1, kDataAttribute);

    shader->enableAttributeArray(location);
    shader->setAttributeBuffer(
        location, GL_DOUBLE, 0, QnColorLineGLShaderProgram::kComponentPerVertex);

    lineBuffer.release();

    lineVertices.release();

    lineVaoInitialized = true;
}

void Renderer::Private::ensureTriangleVao(QnGLShaderProgram* shader)
{
    if (triangleVaoInitialized)
        return;

    triangleVertices.create();
    triangleVertices.bind();

    trianglePosBuffer.create();
    trianglePosBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    trianglePosBuffer.bind();

    static constexpr auto kPositionAttribute = "aPosition";
    const auto posLocation = shader->attributeLocation(kPositionAttribute);
    NX_ASSERT(posLocation != -1, kPositionAttribute);

    std::vector<GLdouble> vertices;
    for (const auto& point: kDirectionMark)
    {
        vertices.push_back(point.x());
        vertices.push_back(point.y());
    }
    trianglePosBuffer.allocate(vertices.data(), sizeof(GLdouble) * vertices.size());

    shader->enableAttributeArray(posLocation);
    shader->setAttributeBuffer(posLocation, GL_DOUBLE, 0, 2);

    trianglePosBuffer.release();
    triangleVertices.release();

    triangleVaoInitialized = true;
}

void Renderer::Private::drawPolyline(
    QPainter* painter,
    QWidget* widget,
    const Points& points,
    const QColor& color,
    int width,
    FigureShapeType type,
    DrawBatchMode mode)
{
    if (fillClosedShapes && type == FigureShapeType::closed)
    {
        static const int kRegionOpacity = 38;
        QPainterPath path;
        path.addPolygon(points);
        const QColor fillColor(color.red(), color.green(), color.blue(), kRegionOpacity);
        painter->fillPath(path, fillColor);
    }

    const auto glWidget = qobject_cast<QOpenGLWidget*>(widget);
    if (!glWidget)
    {
        QPainterPath path;
        path.addPolygon(points);
        if (type == FigureShapeType::closed)
            path.closeSubpath();
        painter->setPen(color);
        painter->drawPath(path);
        return;
    }

    const auto functions = QOpenGLContext::currentContext()->functions();

    if (mode == DrawBatchMode::singleOperation)
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
        points, type == FigureShapeType::closed, width * kLineScale);
    lineBuffer.bind();
    lineBuffer.allocate(triangles.data(), sizeof(GLdouble) * triangles.size());
    lineBuffer.release();

    shader->bind();
    shader->setModelViewProjectionMatrix(
        renderer->getProjectionMatrix() * renderer->getModelViewMatrix());
    shader->setColor(QVector4D(color.redF(), color.greenF(), color.blueF(), painter->opacity()));
    shader->setFeather(kLineFeather);

    lineVertices.bind();
    functions->glDrawArrays(
        GL_TRIANGLES, 0, triangles.size() / QnColorLineGLShaderProgram::kComponentPerVertex);
    lineVertices.release();

    shader->release();

    if (mode == DrawBatchMode::singleOperation)
    {
        functions->glDisable(GL_BLEND);
        functions->glDisable(GL_LINE_SMOOTH);

        QnGlNativePainting::end(painter);
    }
}

void Renderer::Private::drawDirectionMark(
    QPainter* painter,
    QWidget* widget,
    const QPointF& position,
    qreal angle,
    const QColor& color)
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

    triangleVertices.bind();
    functions->glDrawArrays(GL_TRIANGLES, 0, kDirectionMark.size());
    triangleVertices.release();

    shader->release();
    renderer->popModelViewMatrix();
    functions->glDisable(GL_BLEND);
    QnGlNativePainting::end(painter);
}

void Renderer::Private::drawLineMarks(
    QPainter* painter,
    QWidget* widget,
    const Points::value_type& startPoint,
    const Points::value_type& endPoint,
    const QColor& color,
    Polyline::Direction direction)
{
    core::PathUtil pathUtil;
    pathUtil.setPoints({startPoint, endPoint});
    const auto drawDirection =
        [&](const double angleOffset)
        {
            drawDirectionMark(
                painter,
                widget,
                pathUtil.midAnchorPoint(),
                pathUtil.midAnchorPointNormalAngle() + angleOffset,
                color);
        };

    if (direction == Polyline::Direction::left || direction == Polyline::Direction::both)
        drawDirection(0);

    if (direction == Polyline::Direction::right || direction == Polyline::Direction::both)
        drawDirection(M_PI);
}

void Renderer::Private::drawCross(
    QPainter* painter,
    QWidget* widget,
    const QRectF& boundingRect,
    const QColor& color)
{
    const auto& center = boundingRect.center();
    const auto& topLeft = boundingRect.topLeft();
    const auto& bottomRight = boundingRect.bottomRight();
    const Points horizontalPoints =
        {QPointF(topLeft.x(), center.y()), QPointF(bottomRight.x(), center.y())};
    const Points verticalPoints =
        {QPointF(center.x(), topLeft.y()), QPointF(center.x(), bottomRight.y())};

    drawPolyline(painter, widget, horizontalPoints, color, 2, FigureShapeType::closed);
    drawPolyline(painter, widget, verticalPoints, color, 2, FigureShapeType::closed);
}

//-------------------------------------------------------------------------------------------------

Renderer::Renderer(
    int lineWidth,
    bool fillClosedShapes)
    :
    d(new Private(lineWidth, fillClosedShapes))
{
}

Renderer::~Renderer()
{
}

void Renderer::renderFigure(
    QPainter* painter,
    QWidget* widget,
    const FigurePtr& figure,
    const QSizeF& widgetSize)
{
    if (!NX_ASSERT(figure->isValid(), "Invalid figure detected"))
        return;

    const auto type = figure->type();
    const auto& points = figure->points(widgetSize);
    const auto& color = figure->color();

    switch (type)
    {
        case FigureType::invalid:
        case FigureType::dummy:
            return;
        case FigureType::point:
            d->drawCross(painter, widget, figure->visualRect(widgetSize), color);
            return;
        default:
            break;
    }

    const auto endingMode = figure.dynamicCast<OpenShapeFigure>()
        ? FigureShapeType::open
        : FigureShapeType::closed;

    d->drawPolyline(painter, widget, points, color, d->lineWidth, endingMode);

    if (type == FigureType::polyline)
    {
        const auto* polyline = dynamic_cast<Polyline*>(figure.data());
        const auto direction = polyline->direction();
        const auto startPoint = points[directionMarksEdgeStartPointIndex(polyline)];
        const auto endPoint = points[directionMarksEdgeStartPointIndex(polyline) + 1];
        d->drawLineMarks(painter, widget, startPoint, endPoint, color, direction);
    }
}

} // namespace nx::vms::client::desktop::figure
