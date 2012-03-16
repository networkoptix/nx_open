#include "blue_background_painter.h"
#include <cmath> /* For std::fmod. */
#include <QRect>
#include <QRadialGradient>
#include <ui/graphics/painters/radial_gradient_painter.h>
#include <ui/graphics/opengl/gl_shortcuts.h>

Q_GLOBAL_STATIC_WITH_ARGS(QnRadialGradientPainter, radialGradientPainter, (32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0)));

QnBlueBackgroundPainter::QnBlueBackgroundPainter(qreal cycleIntervalSecs):
    m_cycleIntervalSecs(cycleIntervalSecs)
{
    m_timer.start();
}

QnBlueBackgroundPainter::~QnBlueBackgroundPainter() {}

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

    QColor color(5, 5, 50 + 25 * pos, 255);

    QPointF center1(rect.center().x() - pos * rect.width() / 2, rect.center().y());
    QPointF center2(rect.center().x() + pos * rect.width() / 2, rect.center().y());

    qreal radius = qMin(rect.width(), rect.height()) / 1.4142;

#ifdef QN_BACKGROUND_PAINTER_NO_OPENGL
	{
		QRadialGradient radialGrad(center1, radius);
		radialGrad.setColorAt(0, color);
		radialGrad.setColorAt(1, QColor(0, 0, 0, 0));
		painter->fillRect(rect, radialGrad);
	}

	{
		QRadialGradient radialGrad(center2, radius);
		radialGrad.setColorAt(0, color);
		radialGrad.setColorAt(1, QColor(0, 0, 0, 0));
		painter->fillRect(rect, radialGrad);
	}
#else
    painter->beginNativePainting();
    {
        QnRadialGradientPainter *gradientPainter = radialGradientPainter();

        glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glTranslate(center1);
        glScale(radius, radius);
        gradientPainter->paint(color);
        glPopMatrix();

        glPushMatrix();
        glTranslate(center2);
        glScale(radius, radius);
        gradientPainter->paint(color);
        glPopMatrix();

        glPopAttrib();
    }
    painter->endNativePainting();
#endif
}
