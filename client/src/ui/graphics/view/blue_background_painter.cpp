#include <cmath> /* For std::fmod. */
#include <QRect>
#include <QRadialGradient>
#include "blue_background_painter.h"

QnBlueBackgroundPainter::QnBlueBackgroundPainter(qreal cycleIntervalSecs):
    m_cycleIntervalSecs(cycleIntervalSecs)
{
    m_timer.start();
}

qreal QnBlueBackgroundPainter::position()
{
    qreal t = std::fmod(m_timer.elapsed() / 1000.0, m_cycleIntervalSecs) / m_cycleIntervalSecs;

    /* t interval    | Position change 
     * [0, 0.25]     | 0 ->  1        
     * [0.25, 0.75]  | 1 -> -1        
     * [0.75, 1]     |-1 ->  0        
     */

    if(t < 0.25)
        return t * 4;

    if(t < 0.75)
        return 2 - t * 4;

    return -4 + t * 4;
}

void QnBlueBackgroundPainter::installedNotify() {
    view()->setBackgroundBrush(Qt::black);

    QnLayerPainter::installedNotify();
}

void QnBlueBackgroundPainter::drawLayer(QPainter * painter, const QRectF & rect)
{
	qreal pos = position();

	QColor color(10, 10, 110 + 50 * pos, 255); // NO
	//QColor color(0, 100 + 50 * rpos, 160, 255); // trinity

	QPointF center1(rect.center().x() - pos * rect.width() / 2, rect.center().y());
	QPointF center2(rect.center().x() + pos * rect.width() / 2, rect.center().y());

	{
		QRadialGradient radialGrad(center1, qMin(rect.width(), rect.height()) / 1.5);
		radialGrad.setColorAt(0, color);
		radialGrad.setColorAt(1, QColor(0, 0, 0, 0));
		painter->fillRect(rect, radialGrad);
	}

	{
		QRadialGradient radialGrad(center2, qMin(rect.width(), rect.height()) / 1.5);
		radialGrad.setColorAt(0, color);
		radialGrad.setColorAt(1, QColor(0, 0, 0, 0));
		painter->fillRect(rect, radialGrad);
	}
}
