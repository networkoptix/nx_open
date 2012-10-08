#ifndef QN_GL_SHORTCUTS_H
#define QN_GL_SHORTCUTS_H

#include <QtOpenGL>
#include <cmath> /* For std::sin & std::cos. */

template<class T>
struct coord_type;

template<>
struct coord_type<QVector2D> {
    typedef float type;
};

template<>
struct coord_type<QVector3D> {
    typedef float type;
};

template<>
struct coord_type<QVector4D> {
    typedef float type;
};

template<>
struct coord_type<QPointF> {
    typedef qreal type;
};


template<class T> 
inline T polar(typename coord_type<T>::type alpha, typename coord_type<T>::type r);

template<>
inline QVector2D polar<QVector2D>(float alpha, float r) {
    return QVector2D(r * std::cos(alpha), r * std::sin(alpha));
}

template<>
inline QPointF polar<QPointF>(qreal alpha, qreal r) {
    return QPointF(r * std::cos(alpha), r * std::sin(alpha));
}



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

inline void glVertexPolar(qreal alpha, qreal r) {
    glVertex(polar<QPointF>(alpha, r));
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

inline void glTexCoord(float x, float y) {
    glTexCoord2f(x, y);
}

inline void glTexCoord(double x, double y) {
    glTexCoord2d(x, y);
}

inline void glTexCoord(const QPointF &point) {
    glTexCoord(point.x(), point.y());
}

inline void glTranslate(float x, float y) {
    glTranslatef(x, y, 1.0f);
}

inline void glTranslate(double x, double y) {
    glTranslated(x, y, 1.0);
}

inline void glTranslate(float x, float y, float z) {
    glTranslatef(x, y, z);
}

inline void glTranslate(double x, double y, double z) {
    glTranslated(x, y, z);
}

inline void glTranslate(const QPointF &delta) {
    glTranslate(delta.x(), delta.y());
}

inline void glScale(float x, float y) {
    glScalef(x, y, 1.0f);
}

inline void glScale(double x, double y) {
    glScaled(x, y, 1.0);
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

/**
 * This function checks for OpenGL errors, and if the error did occur, writes
 * it out to log.
 * 
 * \param context                       Context where the error may have occured, e.g.
 *                                      OpenGL function name.
 * \returns                             OpenGL error code. 
 */
int glCheckError(const char *context);


#endif // QN_GL_SHORTCUTS_H
