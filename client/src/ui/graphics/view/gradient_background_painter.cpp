#include "gradient_background_painter.h"

#include <cmath> /* For std::fmod. */

#include <QtCore/QRect>
#include <QtGui/QRadialGradient>

#include <ui/common/linear_combination.h>
#include <ui/graphics/painters/radial_gradient_painter.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>

#include "utils/settings.h"

#include <ui/animation/variant_animator.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>

QnGradientBackgroundPainter::QnGradientBackgroundPainter(qreal cycleIntervalSecs, QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_settings(qnSettings),
    m_cycleIntervalSecs(cycleIntervalSecs)
{
    m_backgroundColorAnimator = new VariantAnimator(this);
    m_backgroundColorAnimator->setTargetObject(this);
    m_backgroundColorAnimator->setAccessor(new PropertyAccessor("backgroundColor"));
    m_backgroundColorAnimator->setConverter(new QnColorToVectorConverter());
    m_backgroundColorAnimator->setSpeed(2.0);

    connect(qnSettings->notifier(QnSettings::BACKGROUND_COLOR), SIGNAL(valueChanged(int)),  this,   SLOT(updateBackgroundColor()));
    connect(context()->watcher<QnWorkbenchPanicWatcher>(),      SIGNAL(panicModeChanged()), this,   SLOT(updateBackgroundColor()));

    updateBackgroundColor(false);

    m_timer.start();
}

QnGradientBackgroundPainter::~QnGradientBackgroundPainter() {}

qreal QnGradientBackgroundPainter::position() {
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

QColor QnGradientBackgroundPainter::backgroundColor() const {
    return m_backgroundColor;
}

void QnGradientBackgroundPainter::setBackgroundColor(const QColor &backgroundColor) {
    m_backgroundColor = backgroundColor;
}

void QnGradientBackgroundPainter::updateBackgroundColor(bool animate) {
    QColor backgroundColor;

    if(context()->watcher<QnWorkbenchPanicWatcher>()->isPanicMode()) {
        backgroundColor = QColor(255, 0, 0, 255);
    } else if(m_settings) {
        backgroundColor = m_settings.data()->backgroundColor();
    } else {
        backgroundColor = QColor(0, 0, 0, 0);
    }

    if(animate) {
        m_backgroundColorAnimator->animateTo(backgroundColor);
    } else {
        m_backgroundColor = backgroundColor;
    }
}

void QnGradientBackgroundPainter::drawLayer(QPainter * painter, const QRectF & rect) {
    if(!m_gl) 
        m_gl.reset(new QnGlFunctions(QGLContext::currentContext()));
    if(!(m_gl->features() & QnGlFunctions::ArbPrograms))
        return; /* Don't draw anything on old OpenGL versions. */

    qreal pos = position();

    QColor color = linearCombine(1.0 + 0.5 * pos, backgroundColor());

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
        if(!m_gradientPainter)
            m_gradientPainter.reset(new QnRadialGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0), QGLContext::currentContext()));

        //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glPushMatrix();
        glTranslate(center1);
        glScale(radius, radius);
        m_gradientPainter->paint(color);
        glPopMatrix();

        glPushMatrix();
        glTranslate(center2);
        glScale(radius, radius);
        m_gradientPainter->paint(color);
        glPopMatrix();

        glDisable(GL_BLEND);
        //glPopAttrib();
    }
    painter->endNativePainting();
#endif
}
