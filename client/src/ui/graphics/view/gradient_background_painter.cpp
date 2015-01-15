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

namespace {
    const int sectorCount = 32;
    const int circlesCount = 2;
}

QnGradientBackgroundPainter::QnGradientBackgroundPainter(qreal cycleIntervalSecs, QObject *parent, QnWorkbenchContext *context):
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_backgroundColorAnimator(NULL),
    m_cycleIntervalSecs(cycleIntervalSecs),
    m_rainbow(new QnRainbow(this))
{
    connect(this->context()->instance<QnWorkbenchPanicWatcher>(),   &QnWorkbenchPanicWatcher::panicModeChanged, this,   &QnGradientBackgroundPainter::updateBackgroundColorAnimated);
    connect(qnSettings->notifier(QnClientSettings::BACKGROUND),     &QnPropertyNotifier::valueChanged,          this,   &QnGradientBackgroundPainter::updateBackgroundColorAnimated);

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
    QnClientBackground background = qnSettings->background();

    QColor targetColor;
    int actualAlpha = background.animationCustomColor.isValid()
        ? background.animationCustomColor.alpha()
        : m_colors.normal.alpha();
    
    if(context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode())
        targetColor = m_colors.panic;
    else if (!background.animationEnabled)
        targetColor = QColor();
    else switch (background.animationMode) {
    case Qn::DefaultAnimation:
        targetColor = withAlpha(m_colors.normal, actualAlpha);
        break;
    case Qn::RainbowAnimation:
        targetColor = withAlpha(m_rainbow->currentColor(), actualAlpha);
        break;
    case Qn::CustomAnimation:
        targetColor = background.animationCustomColor;
        break;
    }

    if(animate) {
        backgroundColorAnimator()->animateTo(targetColor);
    } else {
        m_currentColor = targetColor;
    }
}

void QnGradientBackgroundPainter::drawLayer(QPainter *painter, const QRectF &rect) {
    if (!isEnabled())
        return;

    QnClientBackground background = qnSettings->background();

    if (background.animationEnabled
        && background.animationMode == Qn::RainbowAnimation
        && !context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode()
        && !backgroundColorAnimator()->isRunning())
    {
        m_rainbow->advance();
        updateBackgroundColorAnimated();
    }

    if (!m_currentColor.isValid())
        return;

    qreal pos = position();

    QColor color = linearCombine(1.0 + 0.5 * pos, currentColor());

    QPointF center1(rect.center().x() - pos * rect.width() / 2, rect.center().y());
    QPointF center2(rect.center().x() + pos * rect.width() / 2, rect.center().y());

    qreal radius = qMin(rect.width(), rect.height()) / 1.4142;

    if(!m_gradientPainter)
        m_gradientPainter.reset(new QnRadialGradientPainter(sectorCount, QColor(255, 255, 255, 255), QColor(255, 255, 255, 0), QGLContext::currentContext()));

    if(m_gradientPainter->isAvailable()) {

        QnGlNativePainting::begin(QGLContext::currentContext(),painter);
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            auto renderer = QnOpenGLRendererManager::instance(QGLContext::currentContext());
            
            const qreal distance = (center2.x() - center1.x()) / (circlesCount - 1);
            for (int step = 0; step < circlesCount; ++step) {
                QMatrix4x4 m = renderer->pushModelViewMatrix();
                m.translate(center1.x() + step * distance, center1.y());
                m.scale(radius, radius);
                renderer->setModelViewMatrix(m);
                m_gradientPainter->paint(color);
                renderer->popModelViewMatrix();
            }

            glDisable(GL_BLEND);
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
