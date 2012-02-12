#ifndef QN_RADIAL_GRADIENT_PAINTER_H
#define QN_RADIAL_GRADIENT_PAINTER_H

#include <QScopedPointer>

class QnColorShaderProgram;

class QnRadialGradientPainter {
public:
    QnRadialGradientPainter(int sectorCount, const QColor &innerColor, const QColor &outerColor);

    ~QnRadialGradientPainter();

    void paint(const QColor &colorMultiplier);

    void paint();

private:
    unsigned m_list;
    QScopedPointer<QnColorShaderProgram> m_shader;
};


#endif // QN_RADIAL_GRADIENT_PAINTER_H
