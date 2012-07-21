#ifndef QN_RADIAL_GRADIENT_PAINTER_H
#define QN_RADIAL_GRADIENT_PAINTER_H

#include <QtCore/QSharedPointer>
#include <QtGui/QColor>

class QGLContext;

class QnColorShaderProgram;

class QnRadialGradientPainter {
public:
    QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor, const QGLContext *context);
    ~QnRadialGradientPainter();

    void paint(const QColor &colorMultiplier);
    void paint();

private:
    unsigned m_list;
    QSharedPointer<QnColorShaderProgram> m_shader;
};


#endif // QN_RADIAL_GRADIENT_PAINTER_H
