#include "radial_gradient_painter.h"

#include <utils/common/warnings.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>

QnRadialGradientPainter::QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context):
    QnGlFunctions(context),
    m_shader(QnColorShaderProgram::instance(context)),
    m_sectorCount(sectorCount),
    m_innerColor(innerColor),
    m_outerColor(outerColor)
{
    if(context != QGLContext::currentContext())
        qnWarning("Invalid current OpenGL context.");

    QByteArray data;

    /* Generate vertex data. */
    QnGlBufferStream<GLfloat> vertexStream(&data);
    m_vertexOffset = vertexStream.offset();
    vertexStream << QVector2D(0.0f, 0.0f);
    for(int i = 0; i <= sectorCount; i++)
        vertexStream << polar<QVector2D>(2 * M_PI * i / sectorCount, 1.0f);

    /* Generate color data. */
    QnGlBufferStream<GLfloat> colorStream(&data);
    m_colorOffset = colorStream.offset();
    colorStream << m_innerColor;
    for(int i = 0; i <= sectorCount; i++)
        colorStream << m_outerColor;

    /* Push data to GPU. */
    glGenBuffers(1, &m_buffer);
    glBufferData(m_buffer, );
}

QnRadialGradientPainter::~QnRadialGradientPainter() {
    glDeleteBuffers(1, &m_buffer);
}

void QnRadialGradientPainter::paint(const QColor &colorMultiplier) {
    m_shader->bind();
    m_shader->setColorMultiplier(colorMultiplier);

    glBegin(GL_TRIANGLE_FAN);

    m_shader->setColor(m_innerColor);
    glVertex(0.0, 0.0);

    m_shader->setColor(m_outerColor);
    for(int i = 0; i <= m_sectorCount; i++)
        glVertexPolar(2 * M_PI * i / m_sectorCount, 1.0);

    glEnd();

    m_shader->release();
}

void QnRadialGradientPainter::paint() {
    paint(1.0);
}
