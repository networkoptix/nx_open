#include "gradient_background_painter.h"

#include <cmath> /* For std::fmod. */

#include <QtCore/QRect>
#include <QtGui/QRadialGradient>

#include <utils/math/linear_combination.h>
#include <utils/math/color_transformations.h>

#include <client/client_settings.h>

#include <ui/common/color_to_vector_converter.h>
#include <ui/animation/variant_animator.h>
#include <ui/style/globals.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/painters/radial_gradient_painter.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workaround/gl_native_painting.h>
#include <opengl_renderer.h>

QnGradientBackgroundPainter::QnGradientBackgroundPainter(qreal cycleIntervalSecs, QObject *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_backgroundColorAnimator(NULL),
    m_cycleIntervalSecs(cycleIntervalSecs),
    m_rainbow(NULL)
{
    connect(this->context()->instance<QnWorkbenchPanicWatcher>(), &QnWorkbenchPanicWatcher::panicModeChanged, this, &QnGradientBackgroundPainter::updateBackgroundColorAnimated);

    if(qnSettings->isRainbowMode()) {
        m_rainbow = new QnRainbow(this);
    }

        updateBackgroundColor(false);

    m_timer.start();
}

QnGradientBackgroundPainter::~QnGradientBackgroundPainter() {
    return;
}

VariantAnimator *QnGradientBackgroundPainter::backgroundColorAnimator() {
    if(m_backgroundColorAnimator)
        return m_backgroundColorAnimator;

    if(!view() || !view()->scene())
        return NULL;

    AnimationTimer *animationTimer = InstrumentManager::animationTimer(view()->scene());
    if(!animationTimer)
        return NULL;

    m_backgroundColorAnimator = new VariantAnimator(this);
    m_backgroundColorAnimator->setTimer(animationTimer);
    m_backgroundColorAnimator->setTargetObject(this);
    m_backgroundColorAnimator->setAccessor(new PropertyAccessor("currentColor"));
    m_backgroundColorAnimator->setConverter(new QnColorToVectorConverter());
    m_backgroundColorAnimator->setSpeed(2.0);

    if(m_rainbow)
        m_backgroundColorAnimator->setSpeed(0.3);

    return m_backgroundColorAnimator;
}

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

QColor QnGradientBackgroundPainter::currentColor() const {
    return m_currentColor;
}

void QnGradientBackgroundPainter::setCurrentColor(const QColor &backgroundColor) {
    m_currentColor = backgroundColor;
}

const QnBackgroundColors &QnGradientBackgroundPainter::colors() {
    return m_colors;
}

void QnGradientBackgroundPainter::setColors(const QnBackgroundColors &colors) {
    m_colors = colors;
    
    updateBackgroundColor(false);
}

void QnGradientBackgroundPainter::updateBackgroundColor(bool animate) {
    QColor backgroundColor;
    
    if(m_rainbow) {
        backgroundColor = toTransparent(m_rainbow->currentColor(), 0.5);
    } else if(context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode()) {
        backgroundColor = m_colors.panic;
    } else {
        backgroundColor = m_colors.normal;
    }

    if(animate) {
        backgroundColorAnimator()->animateTo(backgroundColor);
    } else {
        m_currentColor = backgroundColor;
    }
}

void QnGradientBackgroundPainter::drawLayer(QPainter *painter, const QRectF &rect) {
    if(m_rainbow && !backgroundColorAnimator()->isRunning()) {
        m_rainbow->advance();
        updateBackgroundColorAnimated();
    }

    qreal pos = position();

    QColor color = linearCombine(1.0 + 0.5 * pos, currentColor());

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
    if(!m_gradientPainter)
        m_gradientPainter.reset(new QnRadialGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0), QGLContext::currentContext()));

    if(m_gradientPainter->isAvailable()) {

    QnGlNativePainting::begin(QGLContext::currentContext(),painter);
        {
            if(!m_gradientPainter)
                m_gradientPainter.reset(new QnRadialGradientPainter(32, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0), QGLContext::currentContext()));

            //glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        QMatrix4x4 m = QnOpenGLRendererManager::instance(QGLContext::currentContext()).getModelViewMatrix();

        //glPushMatrix();

        const int circlesCount = 2;
        const qreal distance = (center2.x() - center1.x()) / (circlesCount - 1);
        for (int step = 0; step < circlesCount; ++step) {
            QnOpenGLRendererManager::instance(QGLContext::currentContext()).getModelViewMatrix().translate(center1.x() + step * distance, center1.y());
            QnOpenGLRendererManager::instance(QGLContext::currentContext()).getModelViewMatrix().scale(radius, radius);
            m_gradientPainter->paint(color);
            QnOpenGLRendererManager::instance(QGLContext::currentContext()).getModelViewMatrix() = m;
        }

            glDisable(GL_BLEND);
            //glPopAttrib();
        }
        QnGlNativePainting::end(painter);
    } else {
        {
            QRadialGradient gradient(center1, radius);
            gradient.setColorAt(0, color);
            gradient.setColorAt(1, QColor(0, 0, 0, 0));
            painter->fillRect(rect, gradient);
        }
        {
            QRadialGradient gradient(center2, radius);
            gradient.setColorAt(0, color);
            gradient.setColorAt(1, QColor(0, 0, 0, 0));
            painter->fillRect(rect, gradient);
        }
    }
}
#endif