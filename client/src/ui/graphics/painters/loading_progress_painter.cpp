#include "loading_progress_painter.h"

#include <cmath> /* For std::fmod. */

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include <ui/graphics/shaders/color_shader_program.h>
#include <utils/math/linear_combination.h>

QnLoadingProgressPainter::QnLoadingProgressPainter(qreal innerRadius, int sectorCount, qreal sectorFill, const QColor &startColor, const QColor &endColor, const QGLContext *context):
    QOpenGLFunctions(context->contextHandle()),
    m_initialized(false),
    m_sectorCount(sectorCount),
    m_shader(QnColorShaderProgram::instance(context))
{
    if(context != QGLContext::currentContext()) {
        qnWarning("Invalid current OpenGL context.");
        return;
    }

    QByteArray data;

    /* Generate vertex data. */
    QnGlBufferStream<GLfloat> vertexStream(&data);
    m_vertexOffset = vertexStream.offset();
    for(int i = 0; i < sectorCount; i++) {
        qreal a0 = 2 * M_PI / sectorCount * i;
        qreal a1 = 2 * M_PI / sectorCount * (i + sectorFill);
        qreal r0 = innerRadius;
        qreal r1 = 1;

        vertexStream 
            << polarToCartesian<QVector2D>(r0, a0)
            << polarToCartesian<QVector2D>(r0, a1)
            << polarToCartesian<QVector2D>(r1, a1)
            << polarToCartesian<QVector2D>(r1, a0);
    }
    m_vertexCount = sectorCount * 4;

    /* Generate color data. */
    QnGlBufferStream<GLfloat> colorStream(&data);
    m_colorOffset = colorStream.offset();
    for(int i = 0; i < sectorCount; i++) {
        qreal k = static_cast<qreal>(i) / (sectorCount - 1);
        QColor color = linearCombine(1 - k, startColor, k, endColor);
        colorStream << color << color << color << color;
    }

    /* Push data to GPU. */
    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    glBufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_initialized = true;
}

QnLoadingProgressPainter::~QnLoadingProgressPainter() {
    if(m_initialized && QOpenGLContext::currentContext())
        glDeleteBuffers(1, &m_buffer);
}

void QnLoadingProgressPainter::paint() {
    paint(0.0, 1.0);
}

void QnLoadingProgressPainter::paint(qreal progress, qreal opacity) {
    if(!m_initialized || !isAvailable())
        return;

    glPushMatrix();
    glRotate(360.0 * static_cast<int>(std::fmod(progress, 1.0) * m_sectorCount) / m_sectorCount, 0.0, 0.0, 1.0);

    m_shader->bind();
    m_shader->setColorMultiplier(QVector4D(1.0, 1.0, 1.0, opacity));
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableVertexAttribArray(m_shader->colorLocation());
    
    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    glVertexPointer(2, GL_FLOAT, 0, reinterpret_cast<const GLvoid *>(m_vertexOffset));
    glVertexAttribPointer(m_shader->colorLocation(), 4, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<const GLvoid *>(m_colorOffset));
    glDrawArrays(GL_QUADS, 0, m_vertexCount);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(m_shader->colorLocation());
    glDisableClientState(GL_VERTEX_ARRAY);

    m_shader->release();

    glPopMatrix();
}

bool QnLoadingProgressPainter::isAvailable() const {
    return hasOpenGLFeature(QOpenGLFunctions::Shaders) && hasOpenGLFeature(QOpenGLFunctions::Buffers);
}
