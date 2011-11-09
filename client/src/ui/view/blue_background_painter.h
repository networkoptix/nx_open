#ifndef QN_BLUE_BACKROUND_PAINTER_H
#define QN_BLUE_BACKROUND_PAINTER_H

#include <QElapsedTimer>
#include "graphics_view.h" /* For QnLayerPainter. */

class QnBlueBackgroundPainter : public QnLayerPainter
{
public:
    /**
     * \param cycleIntervalSecs         Background animation cycle, in seconds.
     */
	QnBlueBackgroundPainter(qreal cycleIntervalSecs);

	virtual void drawLayer(QPainter *painter, const QRectF &rect) override;

protected:
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
