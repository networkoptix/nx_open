#ifndef QN_PAUSED_PAINTER_H
#define QN_PAUSED_PAINTER_H

#include <QtGlobal> /* For qreal. */

class QnPausedPainter {
public:
    QnPausedPainter();

    ~QnPausedPainter();

    void paint(qreal opacity);
};

#endif // QN_PAUSED_PAINTER_H
