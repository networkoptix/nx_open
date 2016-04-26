#include "two_way_audio_widget_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <api/server_rest_connection.h>

#include <ui/animation/variant_animator.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/accessor.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/style/skin.h>
#include <ui/workaround/sharp_pixmap_painting.h>

#include <utils/common/connective.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/util.h>


namespace
{
    /** If mouse button released before this timeout, show hint. */
    const int kHoldTimeoutMs = 500;

    /** How fast hint should be displayed at maximum. */
    const int kShowHintAnimationTimeMs = 500;

    /** Animation speed. 1.0 means fully expand for 1 second. */
    const qreal kShowHintAnimationSpeed = 2.0;

    /** Animation speed. 1.0 means show/hide for 1 second. */
    const qreal kHintOpacityAnimationSpeed = 3.0;

    /* Max visualizer value change per second. */
    const qreal kVisualizerAnimationSpeed = 2.0;

    /** How long hint should be displayed. */
    const int kShowHintMs = 2000;

    /** How long error should be displayed. */
    const int kShowErrorMs = 2000;

    const qreal kHidden = 0.0;
    const qreal kVisible = 1.0;

    void paintVisualizer(QPainter* painter, const QRectF& rect, const VisualizerData& data, const QColor& color)
    {
        if (data.isEmpty())
            return;

        const qreal kOffset = 1.0;
        qreal lineWidth = (rect.width() / data.size()) - kOffset;    /*< offset between lines */
        if (lineWidth < kOffset)
            return;

        qreal midY = rect.center().y();
        qreal maxHeight = rect.height() - (kOffset * 2);

        QPainterPath path;
        for (int i = 0; i < data.size(); ++i)
        {
            float value = data[i];
            qreal lineHeight = qMax(maxHeight * value, kOffset * 2);
            path.addRect(rect.left() + i * (lineWidth + kOffset), midY - (lineHeight / 2), lineWidth, lineHeight);
        }

        painter->fillPath(path, color);
    }

    VisualizerData animateVector(const VisualizerData& prev, const VisualizerData& next, qint64 timeStepMs)
    {
        if (prev.size() != next.size())
            return next;

        const qreal maxChange = qBound(0.0, kVisualizerAnimationSpeed * timeStepMs / 1000, 1.0);

        VisualizerData result(prev.size());
        for (int i = 0; i < prev.size(); ++i)
        {
            auto current = prev[i];
            auto target = next[i];
            auto change = target - current;
            if (change > 0)
                change = qMin(change, maxChange);
            else
                change = qMax(change, -maxChange);
            result[i] = qBound(0.0f, current + change, 1.0f);
        }
        return result;
    }
}


QnTwoWayAudioWidgetPrivate::QnTwoWayAudioWidgetPrivate(QnTwoWayAudioWidget* owner) :
    q_ptr(owner),
    button(new QnImageButtonWidget(lit("two_way_autio"), owner)),
    hint(new GraphicsLabel(owner)),
    camera(),
    colors(),

    m_started(false),
    m_state(OK),
    m_requestHandle(0),
    m_hintTimer(new QTimer(this)),
    m_hintAnimator(nullptr),
    m_hintVisibility(kHidden),
    m_stateTimer(),
    m_visualizerData()
{
    hint->setAcceptedMouseButtons(0);
    hint->setPerformanceHint(GraphicsLabel::PixmapCaching);
    hint->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont f = hint->font();
    f.setBold(true);
    hint->setFont(f);
    hint->setOpacity(kHidden);

    button->setIcon(qnSkin->icon("item/mic.png"));
    button->setCheckable(false);

    connect(button, &QnImageButtonWidget::pressed,  this, &QnTwoWayAudioWidgetPrivate::startStreaming);
    connect(button, &QnImageButtonWidget::released, this, &QnTwoWayAudioWidgetPrivate::stopStreaming);

    m_hintTimer->setInterval(100);
    m_hintTimer->setSingleShot(false);
    connect(m_hintTimer, &QTimer::timeout, this, [this]
    {
        if (!m_stateTimer.isValid())
            return;

        int timeout = 0;
        switch (m_state)
        {
        case OK:
        case Pressed:
            return;
        case Released:
            timeout = kShowHintMs;
            break;
        case Error:
            timeout = kShowErrorMs;
            break;
        default:
            Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid case");
            return;
        }

        if (m_stateTimer.hasExpired(timeout))
            setState(OK);
    });
}

void QnTwoWayAudioWidgetPrivate::startStreaming()
{
    Q_Q(QnTwoWayAudioWidget);
    if (!q->isEnabled())
        return;

    if (!camera)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    if (m_started)
        return;

    setState(Pressed);

    m_requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), true, [this]
        (bool success, rest::Handle handle, const QnJsonRestResult& result)
    {
        if (handle != m_requestHandle)
            return;

        if (m_state != Pressed)
            return;

        if (!success || result.error != QnRestResult::NoError)
        {
            return;
            setHint(result.errorString);
            setState(Error);
            stopStreaming();
        }

    }, QThread::currentThread());

    if (m_requestHandle <= 0)
        return;

    m_started = true;
}

void QnTwoWayAudioWidgetPrivate::stopStreaming()
{
    if (!m_started)
        return;

    if (m_state != Error)
        setState(Released);

    m_started = false;

    if (!camera)
        return;

    auto server = camera->getParentServer();
    if (!server || server->getStatus() != Qn::Online)
        return;

    //TODO: #GDM What should we do if we cannot stop streaming?
    if (m_state != Error)
        m_requestHandle = server->restConnection()->twoWayAudioCommand(camera->getId(), false, rest::ServerConnection::GetCallback());
}

void QnTwoWayAudioWidgetPrivate::setFixedHeight(qreal height)
{
    Q_Q(QnTwoWayAudioWidget);
    qreal leftMargin = height / 2;

    button->setFixedSize(height);
    hint->setMinimumHeight(height);
    hint->setMaximumHeight(height);
    q->setContentsMargins(leftMargin, 0.0, 0.0, 0.0);
    q->updateGeometry();
}

void QnTwoWayAudioWidgetPrivate::paint(QPainter *painter, const QRectF& sourceRect, const QPalette& palette)
{
    QRectF rect(sourceRect);
    qreal w = rect.width() * m_hintVisibility;

    rect.setLeft(qMin(rect.width() - w, button->geometry().left()));

    qreal minSize = button->geometry().width();

    qreal roundness = minSize / 2;
    QPainterPath path;
    path.addRoundedRect(rect, roundness, roundness);

    QBrush bgColor = m_state == Pressed
        ? colors.background
        : palette.window();

    painter->fillPath(path, bgColor);

    if (m_state == Pressed)
    {
        Q_ASSERT_X(m_stateTimer.isValid(), Q_FUNC_INFO, "Make sure timer is valid");

        auto data = QnSpectrumAnalizer::instance()->getSpectrumData().data;
        if (data.isEmpty())
        {
            setHint(tr("Input device is not found."));
            setState(Error);
            return;
        }
//         VisualizerData data;
//         for (int i = 0; i < 64; ++i)
//             data << 1.0f * random(0, 100) / 100.0f;

        qint64 oldTs = m_paintTimeStamp;
        m_paintTimeStamp = m_stateTimer.elapsed();
        m_visualizerData = animateVector(m_visualizerData, data, m_paintTimeStamp - oldTs);

        QRectF visualizerRect(rect.adjusted(roundness, 0.0, -minSize - roundness, 0.0));
        if (visualizerRect.isValid())
            paintVisualizer(painter, visualizerRect, m_visualizerData, colors.visualizer);
    }
}

void QnTwoWayAudioWidgetPrivate::setState(HintState state)
{
    if (m_state == state)
        return;

    m_state = state;

    bool open = false;
    switch (m_state)
    {
    case Pressed:
        {
            open = true;
            setHint(tr("Hold to Speak Something"));
            opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);
        }
        break;
    case Released:
        open = m_stateTimer.isValid() && !m_stateTimer.hasExpired(kHoldTimeoutMs);
        break;
    case Error:
        open = true;
        break;
    default:
        break;
    }

    if (m_state == OK)
        m_stateTimer.invalidate();
    else
        m_stateTimer.restart();

    ensureAnimator();
    disconnect(m_hintAnimator, nullptr, this, nullptr);
    if (open)
    {
        connect(m_hintAnimator, &AbstractAnimator::finished, this, [this]()
        {
            if (m_state == Released || m_state == Error)
                opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kVisible);
        });
        m_hintAnimator->setEasingCurve(QEasingCurve::OutCubic);
        m_hintAnimator->animateTo(kVisible);
        m_hintTimer->start();
    }
    else
    {
        m_hintAnimator->setEasingCurve(QEasingCurve::InCubic);
        m_hintAnimator->animateTo(kHidden);
        m_hintTimer->stop();
        opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);
    }
}

void QnTwoWayAudioWidgetPrivate::setHint(const QString& text)
{
    Q_Q(QnTwoWayAudioWidget);
    hint->setText(text);
    q->updateGeometry();
}

void QnTwoWayAudioWidgetPrivate::ensureAnimator()
{
    if (m_hintAnimator)
        return;

    auto hintCoordGetter = [this](const QObject* target) -> qreal
    {
        Q_UNUSED(target);
        return m_hintVisibility;
    };

    auto hintCoordSetter = [this](const QObject* target, const QVariant& value)
    {
        Q_UNUSED(target);
        m_hintVisibility = value.toDouble();
        Q_Q(QnTwoWayAudioWidget);
        q->update();
    };

    AbstractAccessor* accessor = new AccessorAdaptor<decltype(hintCoordGetter), decltype(hintCoordSetter)>(hintCoordGetter, hintCoordSetter);

    Q_Q(QnTwoWayAudioWidget);
    m_hintAnimator = new VariantAnimator(this);
    m_hintAnimator->setEasingCurve(QEasingCurve::InOutCubic);
    m_hintAnimator->setTimer(InstrumentManager::animationTimer(q->scene()));
    m_hintAnimator->setAccessor(accessor);
    m_hintAnimator->setTargetObject(this);
    m_hintAnimator->setTimeLimit(kShowHintAnimationTimeMs);
    m_hintAnimator->setSpeed(kShowHintAnimationSpeed);
}
