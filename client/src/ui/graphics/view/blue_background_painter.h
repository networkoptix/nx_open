#ifndef QN_BLUE_BACKROUND_PAINTER_H
#define QN_BLUE_BACKROUND_PAINTER_H

#include <QElapsedTimer>
#include <QScopedPointer>
#include "graphics_view.h" /* For QnLayerPainter. */

#define QN_BACKGROUND_PAINTER_NO_OPENGL

class QnBlueBackgroundPainter : public QnLayerPainter
{
public:
    /**
     * \param cycleIntervalSecs         Background animation cycle, in seconds.
     */
    QnBlueBackgroundPainter(qreal cycleIntervalSecs);

    virtual ~QnBlueBackgroundPainter();

    virtual void drawLayer(QPainter *painter, const QRectF &rect) override;

protected:
    virtual void installedNotify() override;

    /** 
     * \returns                         Current animation position, a number in
     *                                  range [-1, 1].
     */
    qreal position();

private:
    QElapsedTimer m_timer;
    qreal m_cycleIntervalSecs;
};

#endif // QN_BLUE_BACKROUND_PAINTER_H
