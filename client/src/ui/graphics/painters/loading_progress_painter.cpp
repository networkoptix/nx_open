#include "loading_progress_painter.h"
#include <cmath> /* For std::fmod. */
#include <ui/common/opengl.h>
#include <ui/common/linear_combination.h>
#include <ui/graphics/shaders/color_shader_program.h>

QnLoadingProgressPainter::QnLoadingProgressPainter(qreal innerRadius, int sectorCount, qreal sectorFill, const QColor &startColor, const QColor &endColor):
    m_sectorCount(sectorCount),
    m_program(new QnColorShaderProgram())
{
    /* Create display list. */
    m_list = glGenLists(1);

    /* Compile the display list. */
    LinearCombinator *combinator = LinearCombinator::forType(QMetaType::QColor);
    glNewList(m_list, GL_COMPILE);
    glBegin(GL_QUADS);
    for(int i = 0; i < sectorCount; i++) {
        qreal a0 = 2 * M_PI / sectorCount * i;
        qreal a1 = 2 * M_PI / sectorCount * (i + sectorFill);
        qreal r0 = innerRadius;
        qreal r1 = 1;

        qreal k = static_cast<qreal>(i) / (sectorCount - 1);
        glColor(combinator->combine(1 - k, startColor, k, endColor).value<QColor>());
        glVertexPolar(a0, r0);
        glVertexPolar(a1, r0);
        glVertexPolar(a1, r1);
        glVertexPolar(a0, r1);
    }
    glEnd();
    glEndList();
}

QnLoadingProgressPainter::~QnLoadingProgressPainter() {
    glDeleteLists(m_list, 1);
}

void QnLoadingProgressPainter::paint() {
    paint(0.0, 1.0);
}

void QnLoadingProgressPainter::paint(qreal progress, qreal opacity) {
    glPushMatrix();
    glRotate(360.0 * static_cast<int>(std::fmod(progress, 1.0) * m_sectorCount) / m_sectorCount, 0.0, 0.0, 1.0);
    m_program->bind();
    m_program->setColorMultiplier(QVector4D(1.0, 1.0, 1.0, opacity));
    glCallList(m_list);
    m_program->release();
    glPopMatrix();
}
