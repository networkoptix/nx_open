#include "radial_gradient_painter.h"

#include <utils/common/warnings.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
//#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/shaders/per_vertex_colored_shader_program.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include "opengl_renderer.h"

QnRadialGradientPainter::QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context):
    QOpenGLFunctions(context->contextHandle()),
    m_initialized(false),
    m_sectorCount(sectorCount),
    m_innerColor(innerColor),
    m_outerColor(outerColor),
    m_positionBuffer(QOpenGLBuffer::VertexBuffer),
    m_colorBuffer(QOpenGLBuffer::VertexBuffer)
{
    if(context != QGLContext::currentContext()) {
        qnWarning("Invalid current OpenGL context.");
        return;
    }
}

QnRadialGradientPainter::~QnRadialGradientPainter() {
}

void QnRadialGradientPainter::paint(const QColor &colorMultiplier) {
    if (!isAvailable())
        return;

    if (!m_initialized)
        initialize();

    auto renderer = QnOpenGLRendererManager::instance(QGLContext::currentContext());
    renderer->setColor(colorMultiplier);
    auto shader = renderer->getColorPerVertexShader();
    renderer->drawArraysVao(&m_vertices, GL_TRIANGLE_FAN, m_sectorCount + 2, shader);
}

void QnRadialGradientPainter::paint() {
    paint(Qt::white);
}

bool QnRadialGradientPainter::isAvailable() const {
    return hasOpenGLFeature(QOpenGLFunctions::Shaders) && hasOpenGLFeature(QOpenGLFunctions::Buffers);
}

void QnRadialGradientPainter::initialize() {

    /* Generate vertex data. */
    QByteArray data;
    QnGlBufferStream<GLfloat> vertexStream(&data);
    vertexStream << QVector2D(0.0f, 0.0f);
    for(int i = 0; i <= m_sectorCount; i++)
        vertexStream << polarToCartesian<QVector2D>(1.0f, 2 * M_PI * i / m_sectorCount);

    /* Generate color data. */
    QByteArray colorData;
    QnGlBufferStream<GLfloat> colorStream(&colorData);
    colorStream << m_innerColor;
    for(int i = 0; i <= m_sectorCount; i++)
        colorStream << m_outerColor;

    m_vertices.create();
    m_vertices.bind();

    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_COLOR_SIZE = 4; // s and t
    const int VERTEX_POS_INDX = 0;
    const int VERTEX_COLOR_INDX = 1;

    auto shader = QnOpenGLRendererManager::instance(QGLContext::currentContext())->getColorPerVertexShader();

    m_positionBuffer.create();
    m_positionBuffer.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_positionBuffer.bind();
    m_positionBuffer.allocate( data.data(), data.size() );
    shader->enableAttributeArray( VERTEX_POS_INDX );
    shader->setAttributeBuffer( VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE );

    m_colorBuffer.create();
    m_colorBuffer.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_colorBuffer.bind();
    m_colorBuffer.allocate( colorData.data(), colorData.size());
    shader->enableAttributeArray( VERTEX_COLOR_INDX );
    shader->setAttributeBuffer( VERTEX_COLOR_INDX, GL_FLOAT, 0, VERTEX_COLOR_SIZE );

    if (!shader->initialized()) {
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        shader->bindAttributeLocation("aColor", VERTEX_COLOR_INDX);
        shader->markInitialized();
    };

    m_positionBuffer.release();
    m_colorBuffer.release();
    m_vertices.release();

    m_initialized = true;
}
