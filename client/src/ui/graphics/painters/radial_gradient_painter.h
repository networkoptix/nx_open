#ifndef QN_RADIAL_GRADIENT_PAINTER_H
#define QN_RADIAL_GRADIENT_PAINTER_H

#include <QtCore/QSharedPointer>
#include <QtGui/QColor>
#include <QtGui/QOpenGLFunctions>

class QGLContext;

class QnRadialGradientPainter: protected QOpenGLFunctions {
public:
    QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context);
    ~QnRadialGradientPainter();

    void paint(const QColor &colorMultiplier);
    void paint();

    bool isAvailable() const;

private:
    void initialize(); 

private:
    bool m_initialized;
    const int m_sectorCount;
    const QColor m_innerColor;
    const QColor m_outerColor;
    QOpenGLVertexArrayObject m_vertices;
    QOpenGLBuffer m_positionBuffer;
    QOpenGLBuffer m_colorBuffer;
};


#endif // QN_RADIAL_GRADIENT_PAINTER_H
