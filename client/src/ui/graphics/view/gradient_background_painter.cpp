#include "gradient_background_painter.h"
#include <cmath> /* For std::fmod. */
#include <QRect>
#include <QRadialGradient>
#include <ui/common/linear_combination.h>
#include <ui/graphics/painters/radial_gradient_painter.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include "utils/settings.h"

Q_GLOBAL_STATIC_WITH_ARGS(QnRadialGradientPainter, radialGradientPainter, (32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0)));

QnGradientBackgroundPainter::QnGradientBackgroundPainter(qreal cycleIntervalSecs):
    m_cycleIntervalSecs(cycleIntervalSecs),
    m_colorCombinator(LinearCombinator::forType(qMetaTypeId<QColor>())),
    m_settings(qnSettings)
{
    m_timer.start();
}

QnGradientBackgroundPainter::~QnGradientBackgroundPainter() {}

qreal QnGradientBackgroundPainter::position()
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

void QnGradientBackgroundPainter::installedNotify() {
    view()->setBackgroundBrush(Qt::black);

    QnLayerPainter::installedNotify();
}

void QnGradientBackgroundPainter::drawLayer(QPainter * painter, const QRectF & rect)
{
    if(!m_settings->isBackgroundAnimated())
        return;

    qreal pos = position();

    QColor color = m_colorCombinator->combine(1.0 + 0.5 * pos, m_settings->backgroundColor()).value<QColor>();

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

        //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
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

        glDisable(GL_BLEND);
        //glPopAttrib();
    }
    painter->endNativePainting();
#endif
}
