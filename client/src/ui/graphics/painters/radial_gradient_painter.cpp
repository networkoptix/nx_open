#include "radial_gradient_painter.h"

#include <utils/common/warnings.h>

#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/shaders/color_shader_program.h>

QnRadialGradientPainter::QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context):
    m_shader(new QnColorShaderProgram(context)) 
{
    if(context != QGLContext::currentContext())
        qnWarning("Invalid current OpenGL context.");

    /* Create display list. */
    m_list = glGenLists(1);

    /* Compile the display list. */
    glNewList(m_list, GL_COMPILE);
    glBegin(GL_TRIANGLE_FAN);
    
    glColor(innerColor);
    glVertex(0.0, 0.0);

    glColor(outerColor);
    for(int i = 0; i <= sectorCount; i++)
        glVertexPolar(2 * M_PI * i / sectorCount, 1.0);

    glEnd();
    glEndList();
}

QnRadialGradientPainter::~QnRadialGradientPainter() {
    glDeleteLists(m_list, 1);
}

void QnRadialGradientPainter::paint(const QColor &colorMultiplier) {
    m_shader->bind();
    m_shader->setColorMultiplier(colorMultiplier);
    glCallList(m_list);
    m_shader->release();
}

void QnRadialGradientPainter::paint() {
    paint(1.0);
}
