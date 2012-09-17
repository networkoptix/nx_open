#ifndef QN_PAUSED_PAINTER_H
#define QN_PAUSED_PAINTER_H

#include <QtCore/QSharedPointer>

class QGLContext;

class QnColorShaderProgram;

class QnPausedPainter {
public:
    QnPausedPainter(const QGLContext *context);

    ~QnPausedPainter();

    void paint(qreal opacity);

    void paint();

private:
    unsigned m_list;
    QSharedPointer<QnColorShaderProgram> m_program;
};

#endif // QN_PAUSED_PAINTER_H
