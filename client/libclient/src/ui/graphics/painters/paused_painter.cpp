#include "paused_painter.h"

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
//#include <ui/graphics/shaders/color_shader_program.h>
#include "opengl_renderer.h"

QnPausedPainter::QnPausedPainter(const QGLContext *context):
    QOpenGLFunctions(context->contextHandle()),
    //m_shader(QnColorShaderProgram::instance(context)),
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
    GLfloat d = 1.0f / 3.0f;
    vertexStream 
        << -1.0 << -1.0 //0
        << -d   << -1.0 //1
        << -d   <<  1.0 //2

        << -d   <<  1.0 //2
        << -1.0 <<  1.0 //3
        << -1.0 << -1.0 //0

        <<  d   << -1.0 //0
        <<  1.0 << -1.0 //1
        <<  1.0 <<  1.0 //2

        <<  1.0 <<  1.0 //2
        <<  d   <<  1.0 //3
        <<  d   << -1.0;//0
    m_vertexCount = 12;
    
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

    m_initialized = true;
}

QnPausedPainter::~QnPausedPainter() {
    if(m_initialized && QOpenGLContext::currentContext())
        glDeleteBuffers(1, &m_buffer);
}

void QnPausedPainter::paint(qreal opacity) {
    if(!m_initialized)
        return;
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->setColor(QVector4D(1.0, 1.0, 1.0, opacity));
    QnOpenGLRendererManager::instance(QGLContext::currentContext())->drawPerVertexColoredPolygon(m_buffer,m_vertexCount,GL_TRIANGLES);
}

void QnPausedPainter::paint() {
    paint(1.0);
}
