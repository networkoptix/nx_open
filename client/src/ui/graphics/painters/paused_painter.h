#ifndef QN_PAUSED_PAINTER_H
#define QN_PAUSED_PAINTER_H

#include <QScopedPointer>

class QnColorShaderProgram;

class QnPausedPainter {
public:
    QnPausedPainter();

    ~QnPausedPainter();

    void paint(qreal opacity);

    void paint();

private:
    unsigned m_list;
    QScopedPointer<QnColorShaderProgram> m_program;
};

#endif // QN_PAUSED_PAINTER_H
