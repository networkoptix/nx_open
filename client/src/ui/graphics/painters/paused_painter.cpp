#include "paused_painter.h"

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include <ui/graphics/shaders/color_shader_program.h>

QnPausedPainter::QnPausedPainter(const QGLContext *context):
    QnGlFunctions(context),
    m_shader(QnColorShaderProgram::instance(context)) 
{
    if(context != QGLContext::currentContext())
        qnWarning("Invalid current OpenGL context.");

    QByteArray data;

    /* Generate vertex data. */
    QnGlBufferStream<GLfloat> vertexStream(&data);
    m_vertexOffset = vertexStream.offset();
    GLfloat d = 1.0 / 3.0;
    vertexStream 
        << -1.0 << -1.0
        << -d   << -1.0
        << -d   <<  1.0
        << -1.0 <<  1.0
        <<  d   << -1.0
        <<  1.0 << -1.0
        <<  1.0 <<  1.0
        <<  d   <<  1.0;
    m_vertexCount = 8;

    /* Generate color data. */
    QnGlBufferStream<GLfloat> colorStream(&data);
    m_colorOffset = colorStream.offset();
    for(int i = 0; i < m_vertexCount; i++)
        colorStream << QColor(255, 255, 255, 255);

    /* Push data to GPU. */
    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
    glBufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

QnPausedPainter::~QnPausedPainter() {
    glDeleteBuffers(1, &m_buffer);
}

void QnPausedPainter::paint(qreal opacity) {
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
}

void QnPausedPainter::paint() {
    paint(1.0);
}
