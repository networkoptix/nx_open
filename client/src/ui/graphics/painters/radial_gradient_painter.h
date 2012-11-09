#ifndef QN_RADIAL_GRADIENT_PAINTER_H
#define QN_RADIAL_GRADIENT_PAINTER_H

#include <QtCore/QSharedPointer>
#include <QtGui/QColor>

#include <ui/graphics/opengl/gl_functions.h>

class QGLContext;

class QnColorShaderProgram;

class QnRadialGradientPainter: public QnGlFunctions {
public:
    QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context);
    ~QnRadialGradientPainter();

    void paint(const QColor &colorMultiplier);
    void paint();

private:
    GLuint m_buffer;
    int m_vertexOffset, m_colorOffset, m_vertexCount;
    QSharedPointer<QnColorShaderProgram> m_shader;
};


#endif // QN_RADIAL_GRADIENT_PAINTER_H
