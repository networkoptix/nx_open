#ifndef QN_QT_OPENGL_H
#define QN_QT_OPENGL_H

#include <QtOpenGL>

inline void glColor(float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
}

inline void glColor(double r, double g, double b, double a) {
    glColor4d(r, g, b, a);
}

inline void glColor(const QColor &color) {
    glColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

inline void glVertex(float x, float y) {
    glVertex2f(x, y);
}

inline void glVertex(double x, double y) {
    glVertex2d(x, y);
}

inline void glVertex(const QPointF &point) {
    glVertex(point.x(), point.y());
}

inline void glVertices(const QRectF &rect) {
    glVertex(rect.topLeft());
    glVertex(rect.topRight());
    glVertex(rect.bottomRight());
    glVertex(rect.bottomLeft());
}

inline void glVertices(const QPolygonF &polygon) {
    foreach(const QPointF &point, polygon)
        glVertex(point);
}

#endif // QN_QT_OPENGL_H
