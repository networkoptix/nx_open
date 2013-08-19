#ifndef QN_PAUSED_PAINTER_H
#define QN_PAUSED_PAINTER_H

#include <QtCore/QSharedPointer>
#include <QtGui/QOpenGLFunctions>

class QGLContext;

class QnColorShaderProgram;

class QnPausedPainter: protected QOpenGLFunctions {
public:
    QnPausedPainter(const QGLContext *context);

    ~QnPausedPainter();

    void paint(qreal opacity);

    void paint();

private:
    bool m_initialized;
    GLuint m_buffer;
    int m_vertexOffset, m_colorOffset, m_vertexCount;
    QSharedPointer<QnColorShaderProgram> m_shader;
};

#endif // QN_PAUSED_PAINTER_H
