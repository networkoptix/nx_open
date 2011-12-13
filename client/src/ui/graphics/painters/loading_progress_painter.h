#ifndef QN_LOADING_PROGRESS_PAINTER_H
#define QN_LOADING_PROGRESS_PAINTER_H

#include <QColor>
#include <QScopedPointer>

class QGLShaderProgram;

class QnLoadingProgressPainter {
public:
    /**
     * \param innerRadius           Number in [0, 1] defining the inner radius of the progress sign.
     * \param sectorCount           Number of sectors to use.
     * \param sectorFill            Number in [0, 1] defining the relative size of the part of the sector that will be filled.
     * \param startColor            Start fill color.
     * \param endColor              End fill color.
     */
    QnLoadingProgressPainter(qreal innerRadius, int sectorCount, qreal sectorFill, const QColor &startColor, const QColor &endColor);

    ~QnLoadingProgressPainter();

    void paint();

    void paint(qreal progress, qreal opacity);

private:
    int m_sectorCount;
    unsigned m_list;
    QScopedPointer<QGLShaderProgram> m_program;
    int m_colorMultiplierLocation;
};

#endif // QN_LOADING_PROGRESS_PAINTER_H
