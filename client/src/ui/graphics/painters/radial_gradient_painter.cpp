#include "radial_gradient_painter.h"

#include <utils/common/warnings.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
//#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include "opengl_renderer.h"

QnRadialGradientPainter::QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context):
    QOpenGLFunctions(context->contextHandle()),
    m_initialized(false)
{
    if(context != QGLContext::currentContext()) {
        qnWarning("Invalid current OpenGL context.");
        return;
    }

    QByteArray data;
    /* Generate vertex data. */
    QnGlBufferStream<GLfloat> vertexStream(&data);
    m_vertexOffset = vertexStream.offset();
    vertexStream << QVector2D(0.0f, 0.0f);
    for(int i = 0; i <= sectorCount; i++)
        vertexStream << polarToCartesian<QVector2D>(1.0f, 2 * M_PI * i / sectorCount);
    m_vertexCount = sectorCount + 2;

    /* Generate color data. */
    QnGlBufferStream<GLfloat> colorStream(&data);
    m_colorOffset = colorStream.offset();
    colorStream << innerColor;
    for(int i = 0; i <= sectorCount; i++)
        colorStream << outerColor;

    /* Push data to GPU. */
    
    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    glBufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    m_initialized = true;
}

QnRadialGradientPainter::~QnRadialGradientPainter() {
    if(m_initialized && QGLContext::currentContext())
        glDeleteBuffers(1, &m_buffer);
}

void QnRadialGradientPainter::paint(const QColor &colorMultiplier) {
    if(!m_initialized)
        return;

    QnOpenGLRendererManager::instance(QGLContext::currentContext()).setColor(colorMultiplier);
    QnOpenGLRendererManager::instance(QGLContext::currentContext()).drawPerVertexColoredPolygon(m_buffer,m_vertexCount);
}

void QnRadialGradientPainter::paint() {
    paint(Qt::white);
}
