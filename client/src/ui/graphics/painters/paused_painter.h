#ifndef QN_PAUSED_PAINTER_H
#define QN_PAUSED_PAINTER_H

#include <QtCore/QSharedPointer>

#include <ui/graphics/opengl/gl_functions.h>

class QGLContext;

class QnColorShaderProgram;

class QnPausedPainter: public QnGlFunctions {
public:
    QnPausedPainter(const QGLContext *context);

    ~QnPausedPainter();

    void paint(qreal opacity);

    void paint();

private:
    GLuint m_buffer;
    int m_vertexOffset, m_colorOffset, m_vertexCount;
    QSharedPointer<QnColorShaderProgram> m_shader;
};

#endif // QN_PAUSED_PAINTER_H
