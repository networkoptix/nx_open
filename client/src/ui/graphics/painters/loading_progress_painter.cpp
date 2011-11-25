#include "loading_progress_painter.h"
#include <cmath> /* For std::fmod, std::sin and std::cos. */
#include <utils/common/qt_opengl.h>

namespace {
    void glVertexCircular(qreal alpha, qreal r) {
        glVertex(r * std::cos(alpha), r * std::sin(alpha));
    }

    template<class T> 
    T interpolate(const T &f, const T &t, qreal progress) {
        return T(f + (t - f) * progress);
    }

    QColor interpolate(const QColor &f,const QColor &t, qreal progress) {
        return QColor(
            qBound(0, interpolate(f.red(),   t.red(),   progress), 255),
            qBound(0, interpolate(f.green(), t.green(), progress), 255),
            qBound(0, interpolate(f.blue(),  t.blue(),  progress), 255),
            qBound(0, interpolate(f.alpha(), t.alpha(), progress), 255)
        );
    }

} // anonymous namespace


QnLoadingProgressPainter::QnLoadingProgressPainter(qreal innerRadius, int sectorCount, qreal sectorFill, const QColor &startColor, const QColor &endColor):
    m_sectorCount(sectorCount)
{
    /* Create display list. */
    m_list = glGenLists(1);

    /* Compile the display list. */
    glNewList(m_list, GL_COMPILE);
    glBegin(GL_QUADS);
    for(int i = 0; i < sectorCount; i++) {
        qreal a0 = 2 * M_PI / sectorCount * i;
        qreal a1 = 2 * M_PI / sectorCount * (i + sectorFill);
        qreal r0 = innerRadius;
        qreal r1 = 1;

        glColor(interpolate(startColor, endColor, static_cast<qreal>(i) / (sectorCount - 1)));
        glVertexCircular(a0, r0);
        glVertexCircular(a1, r0);
        glVertexCircular(a1, r1);
        glVertexCircular(a0, r1);
    }
    glEnd();
    glEndList();
}

QnLoadingProgressPainter::~QnLoadingProgressPainter() {
    glDeleteLists(m_list, 1);
}

void QnLoadingProgressPainter::paint() {
    glCallList(m_list);
}

void QnLoadingProgressPainter::paint(qreal progress) {
    glPushMatrix();
    glRotate(360.0 * static_cast<int>(std::fmod(progress, 1.0) * m_sectorCount) / m_sectorCount, 0.0, 0.0, 1.0);
    paint();
    glPopMatrix();
}
