#include "paused_painter.h"
#include <utils/common/qt_opengl.h>
#include "color_shader_program.h"

QnPausedPainter::QnPausedPainter():
    m_program(new QnColorShaderProgram()) 
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
