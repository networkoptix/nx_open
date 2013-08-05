#include "radial_gradient_painter.h"

#include <utils/common/warnings.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/shaders/color_shader_program.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>

QnRadialGradientPainter::QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context):
    QnGlFunctions(context),
    m_initialized(false),
    m_shader(QnColorShaderProgram::instance(context))
{
    if(context != QGLContext::currentContext()) {
        qnWarning("Invalid current OpenGL context.");
        return;
    }

    if(!(features() & OpenGL2_0)) {
        qnWarning("OpenGL version 2.0 is required.");
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
    if(m_initialized)
        glDeleteBuffers(1, &m_buffer);
}

void QnRadialGradientPainter::paint(const QColor &colorMultiplier) {
    if(!m_initialized)
        return;

    m_shader->bind();
    m_shader->setColorMultiplier(colorMultiplier);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableVertexAttribArray(m_shader->colorLocation());

    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    glVertexPointer(2, GL_FLOAT, 0, reinterpret_cast<const GLvoid *>(m_vertexOffset));
    glVertexAttribPointer(m_shader->colorLocation(), 4, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLvoid *>(m_colorOffset));
    glDrawArrays(GL_TRIANGLE_FAN, 0, m_vertexCount);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(m_shader->colorLocation());
    glDisableClientState(GL_VERTEX_ARRAY);

    m_shader->release();
}

void QnRadialGradientPainter::paint() {
    paint(1.0);
}
