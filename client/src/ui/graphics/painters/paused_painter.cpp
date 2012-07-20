#include "paused_painter.h"

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/shaders/color_shader_program.h>

QnPausedPainter::QnPausedPainter(const QGLContext *context):
    m_program(QnColorShaderProgram::instance(context)) 
{
    /* Create display list. */
    m_list = glGenLists(1);

    /* Compile the display list. */
    glNewList(m_list, GL_COMPILE);
    glBegin(GL_QUADS);
    glColor(1.0, 1.0, 1.0, 1.0);

    qreal d = 1.0 / 3.0;
    glVertex(-1.0, -1.0);
    glVertex(-d,   -1.0);
    glVertex(-d,    1.0);
    glVertex(-1.0,  1.0);
    glVertex( d,   -1.0);
    glVertex( 1.0, -1.0);
    glVertex( 1.0,  1.0);
    glVertex( d,    1.0);
    glEnd();
    glEndList();
}

QnPausedPainter::~QnPausedPainter() {
    glDeleteLists(m_list, 1);
}

void QnPausedPainter::paint(qreal opacity) {
    m_program->bind();
    m_program->setColorMultiplier(QVector4D(1.0, 1.0, 1.0, opacity));
    glCallList(m_list);
    m_program->release();
}

void QnPausedPainter::paint() {
    paint(1.0);
}
