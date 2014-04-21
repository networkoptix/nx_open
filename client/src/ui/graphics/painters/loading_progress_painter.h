#ifndef QN_LOADING_PROGRESS_PAINTER_H
#define QN_LOADING_PROGRESS_PAINTER_H

#include <QtCore/QSharedPointer>
#include <QtGui/QColor>
#include <QtGui/QOpenGLFunctions>

class QGLContext;

//class QnColorShaderProgram;

class QnLoadingProgressPainter: protected QOpenGLFunctions {
public:
    /**
     * \param innerRadius           Number in [0, 1] defining the inner radius of the progress sign.
     * \param sectorCount           Number of sectors to use.
     * \param sectorFill            Number in [0, 1] defining the relative size of the part of the sector that will be filled.
     * \param startColor            Start fill color.
     * \param endColor              End fill color.
     * \param context               OpenGL context in which this painter will be created.
     */
    QnLoadingProgressPainter(qreal innerRadius, int sectorCount, qreal sectorFill, const QColor &startColor, const QColor &endColor, const QGLContext *context);

    ~QnLoadingProgressPainter();

    void paint();
    void paint(qreal progress, qreal opacity);

    bool isAvailable() const;

private:
    bool m_initialized;
    GLuint m_buffer;
    int m_vertexOffset, m_colorOffset, m_vertexCount;
    int m_sectorCount;
    //QSharedPointer<QnColorShaderProgram> m_shader;
};

#endif // QN_LOADING_PROGRESS_PAINTER_H
