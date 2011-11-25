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

inline void glTranslate(float x, float y, float z) {
    glTranslatef(x, y, z);
}

inline void glTranslate(double x, double y, double z) {
    glTranslated(x, y, z);
}

inline void glScale(float x, float y, float z) {
    glScalef(x, y, z);
}

inline void glScale(double x, double y, double z) {
    glScaled(x, y, z);
}

inline void glRotate(float angle, float x, float y, float z) {
    glRotatef(angle, x, y, z);
}

inline void glRotate(double angle, double x, double y, double z) {
    glRotated(angle, x, y, z);
}


#endif // QN_QT_OPENGL_H
