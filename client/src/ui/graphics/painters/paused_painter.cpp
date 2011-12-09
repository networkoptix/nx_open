#include "paused_painter.h"
#include <utils/common/qt_opengl.h>

QnPausedPainter::QnPausedPainter() {}

QnPausedPainter::~QnPausedPainter() {}

void QnPausedPainter::paint(qreal opacity) {
    glBegin(GL_QUADS);
    glColor(1.0, 1.0, 1.0, opacity);
    
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
}
