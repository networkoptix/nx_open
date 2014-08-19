#ifndef QN_RADIAL_GRADIENT_PAINTER_H
#define QN_RADIAL_GRADIENT_PAINTER_H

#include <QtCore/QSharedPointer>
#include <QtGui/QColor>
#include <QtGui/QOpenGLFunctions>

class QGLContext;

class QnColorGLShaderProgramm;
class QnPerVertexColoredGLShaderProgramm;

class QnRadialGradientPainter: protected QOpenGLFunctions {
public:
    QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context);
    ~QnRadialGradientPainter();

    void paint(const QColor &colorMultiplier);
    void paint();

    bool isAvailable() const;

private:
    bool m_initialized;
    GLuint m_buffer;
    int m_vertexOffset, m_colorOffset, m_vertexCount;
    QOpenGLVertexArrayObject m_vertices;
    QnPerVertexColoredGLShaderProgramm* m_shader;
    QOpenGLBuffer m_positionBuffer;
    QOpenGLBuffer m_colorBuffer;
    //QSharedPointer<QnColorGLShaderProgramm> m_shader;
};


#endif // QN_RADIAL_GRADIENT_PAINTER_H
