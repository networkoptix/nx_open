#include "two_way_audio_widget_p.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
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
#include <utils/license_usage_helper.h>

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

    /** How opaque should enabled button be painted. */
    const qreal kEnabledOpacityCoeff = 1.0;

    /** How opaque should disabled button be painted. */
    const qreal kDisabledOpacityCoeff = 0.3;

    /** Label font size. */
    const int kHintFontPixelSize = 14;

    /** Max visualizer value change per second (on increasing and decreasing values). */
    const qreal kVisualizerAnimationUpSpeed = 5.0;
    const qreal kVisualizerAnimationDownSpeed = 1.0;

    /** Maximum visualizer height. */
    const qreal kMaxVisualizerHeightCoeff = 0.9;

    /** Values lower than that value will be counted as silence. */
    const qreal kNormalizerSilenceValue = 0.1;

    /** Maximum value, to which all values are normalized. */
    const qreal kNormalizerIncreaseValue = 0.9;

    /** Recommended visualizer line width. */
    const qreal kVisualizerLineWidth = 4.0;

    /** Visualizer offset between lines. */
    const qreal kVisualizerLineOffset = 2.0;

    /** How long hint should be displayed. */
    const int kShowHintMs = 2000;

    /** How long error should be displayed. */
    const int kShowErrorMs = 2000;

    /** How fast should we warn user about absent data. */
    const int kDataTimeoutMs = 2000;

    const qreal kHidden = 0.0;
    const qreal kVisible = 1.0;

    void paintVisualizer(QPainter* painter, const QRectF& rect, const VisualizerData& data, const QColor& color)
    {
        if (data.isEmpty())
            return;

        qreal lineWidth = qRound(qMax(kVisualizerLineWidth, (rect.width() / data.size()) - kVisualizerLineOffset));

        qreal midY = rect.center().y();
        qreal maxHeight = rect.height() * kMaxVisualizerHeightCoeff;

        QPainterPath path;
        for (int i = 0; i < data.size(); ++i)
        {
            float value = data[i];
            qreal lineHeight = qRound(qMax(maxHeight * value, kVisualizerLineOffset * 2));
            path.addRect(qRound(rect.left() + i * (lineWidth + kVisualizerLineOffset)), qRound(midY - (lineHeight / 2)), lineWidth, lineHeight);
        }

        paintSharp(painter, [&path, &color](QPainter* painter)
            {
                painter->fillPath(path, color);
            });
    }

    void normalizeVector(VisualizerData& source)
    {
        if (source.isEmpty())
            return;
        auto max = std::max_element(source.cbegin(), source.cend());

        /* Do not normalize if silence */
        if (*max < kNormalizerSilenceValue)
            return;

        /* Do not normalize if there is bigger value, so normalizing will always only increase values. */
        if (*max > kNormalizerIncreaseValue)
            return;

        auto k = kNormalizerIncreaseValue / *max;
        for (auto &e: source)
            e *= k;
    }

    VisualizerData animateVector(const VisualizerData& prev, const VisualizerData& next, qint64 timeStepMs)
    {
        //NX_ASSERT(next.size() == QnVoiceSpectrumAnalyzer::bandsCount());

        if (prev.size() != next.size())
            return next;

        const qreal maxUpChange = qBound(0.0, kVisualizerAnimationUpSpeed * timeStepMs / 1000, 1.0);
        const qreal maxDownChange = qBound(0.0, kVisualizerAnimationDownSpeed * timeStepMs / 1000, 1.0);

        VisualizerData result(prev.size());
        for (int i = 0; i < prev.size(); ++i)
        {
            auto current = prev[i];
            auto target = next[i];
            auto change = target - current;
            if (change > 0)
                change = qMin(change, maxUpChange);
            else
                change = qMax(change, -maxDownChange);
            result[i] = qBound(0.0, current + change, 1.0);
        }
        return result;
    }

    VisualizerData generateEmptyVector(qint64 elapsedMs)
    {
        /* Making slider move forth and back.. */
        int size = QnVoiceSpectrumAnalyzer::bandsCount();
        int maxIdx = size * 2 - 1;

        VisualizerData result(QnVoiceSpectrumAnalyzer::bandsCount(), 0.0);
        int idx = qRound(16.0 * elapsedMs / 1000) % maxIdx;
        if (idx >= size)
            idx = maxIdx - idx;

        bool isValidIndex = idx >= 0 && idx < result.size();
        NX_ASSERT(isValidIndex, Q_FUNC_INFO, "Invalid timeStep value");
        if (isValidIndex)
            result[idx] = 0.2;
        return result;
    }
}


QnTwoWayAudioWidgetPrivate::QnTwoWayAudioWidgetPrivate(
    const QString& sourceId,
    QnTwoWayAudioWidget* owner)
    :
    base_type(),
    button(new QnImageButtonWidget(owner)),
    hint(new GraphicsLabel(owner)),
    colors(),

    q_ptr(owner),

    m_sourceId(sourceId),
    m_started(false),
    m_state(OK),
    m_requestHandle(0),
    m_hintTimer(new QTimer(this)),
    m_hintAnimator(nullptr),
    m_hintVisibility(kHidden),
    m_stateTimer(),
    m_visualizerData(),
    m_paintTimeStamp(0),
    m_camera(),
    m_licenseHelper()
{
    hint->setAcceptedMouseButtons(0);
    hint->setPerformanceHint(GraphicsLabel::PixmapCaching);
    hint->setAlignment(Qt::AlignCenter);
    QFont f = hint->font();
    f.setBold(true);
    f.setPixelSize(kHintFontPixelSize);
    hint->setFont(f);
    hint->setOpacity(kHidden);

    button->setIcon(qnSkin->icon("item/mic.png"));
    button->setCheckable(false);

    connect(button, &QnImageButtonWidget::pressed,  this, &QnTwoWayAudioWidgetPrivate::startStreaming);
    connect(button, &QnImageButtonWidget::pressed,  owner, &QnTwoWayAudioWidget::pressed);
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
            return;

        case Pressed:
            if (m_stateTimer.hasExpired(kDataTimeoutMs))
            {
                auto data = QnVoiceSpectrumAnalyzer::instance()->getSpectrumData().data;
                if (data.isEmpty())
                {
                    setHint(tr("Input device is not selected"));
                    setState(Error);
                    stopStreaming();
                }
            }
            return;

        case Released:
            timeout = kShowHintMs;
            break;

        case Error:
            timeout = kShowErrorMs;
            break;

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Invalid case");
            return;
        }

        if (m_stateTimer.hasExpired(timeout))
            setState(OK);
    });
}

QnTwoWayAudioWidgetPrivate::~QnTwoWayAudioWidgetPrivate()
{
}

void QnTwoWayAudioWidgetPrivate::updateCamera(const QnVirtualCameraResourcePtr& camera)
{
    NX_ASSERT(!m_camera);
    NX_ASSERT(camera);

    if (m_camera)
        disconnect(m_camera, nullptr, this, nullptr);

    m_camera = camera;

    /* If the given camera is I/O Module, then the license is required. */
    if (camera && m_camera->isIOModule())
        m_licenseHelper.reset(new QnSingleCamLicenseStatusHelper(m_camera));
    else
        m_licenseHelper.reset();

    auto updateState = [this]
    {
        Q_Q(QnTwoWayAudioWidget);
        bool enabled = isAllowed();
        q->setEnabled(enabled);
        button->setEnabled(enabled);
        if (!enabled)
            stopStreaming();

        q->setOpacity(enabled ? kEnabledOpacityCoeff : kDisabledOpacityCoeff);
    };

    if (camera)
        connect(camera, &QnResource::statusChanged, this, updateState);
    if (m_licenseHelper)
    {
        connect(m_licenseHelper, &QnSingleCamLicenseStatusHelper::licenseStatusChanged, this,
            updateState);
    }

    updateState();
}

bool QnTwoWayAudioWidgetPrivate::isAllowed() const
{
    if (!m_camera)
        return false;

    bool allowedStatus = (m_camera->getStatus() == Qn::Online || m_camera->getStatus() == Qn::Recording);
    if (!allowedStatus)
        return false;

    /* Check if we are require licenses for two-way audio. */
    if (m_licenseHelper)
        return m_licenseHelper->status() == QnLicenseUsageStatus::used;

    return true;
}

void QnTwoWayAudioWidgetPrivate::startStreaming()
{
    if (!isAllowed())
        return;

    if (!m_camera)
        return;

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->remoteGUID());
    if (!server || server->getStatus() != Qn::Online)
        return;

    if (m_started)
        return;

    setState(Pressed);

    m_requestHandle = server->restConnection()->twoWayAudioCommand(m_sourceId,
        m_camera->getId(),
        true,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (handle != m_requestHandle)
                return;

            if (m_state != Pressed)
                return;

            if (!success || result.error != QnRestResult::NoError)
            {
                setHint(tr("Streaming is not ready yet"));
                setState(Error);
                stopStreaming();
            }

        }, QThread::currentThread());

    m_started = m_requestHandle > 0;
    if (!m_started)
    {
        setHint(tr("Network error"));
        setState(Error);
    }
}

void QnTwoWayAudioWidgetPrivate::stopStreaming()
{
    if (!m_started)
        return;

    NX_ASSERT(m_state == Pressed || m_state == Error, Q_FUNC_INFO, "Invalid state");
    if (m_state != Error)
        setState(Released);

    m_started = false;
    m_requestHandle = 0;
    QnVoiceSpectrumAnalyzer::instance()->reset();
    m_visualizerData = VisualizerData();

    if (!m_camera)
        return;

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->remoteGUID());
    if (!server || server->getStatus() != Qn::Online)
        return;

    // TODO: #GDM What should we do if we cannot stop streaming?

    /* Sending stop anyway, because we can get here in 'Streaming is not ready' error state. */
    server->restConnection()->twoWayAudioCommand(m_sourceId, m_camera->getId(), false,
        rest::ServerConnection::GetCallback());
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
    const qreal minSize = button->geometry().width();
    const qreal minLeftValue = sourceRect.left();               /*< Left value if hint is visible. */
    const qreal maxLeftValue = sourceRect.width() - minSize;    /*< Left value if hint is hidden. */

    const qreal maxHintWidth = (maxLeftValue - minLeftValue);
    const qreal targetHintWidth = maxHintWidth * m_hintVisibility;

    QRectF rect(sourceRect);
    rect.setLeft(minLeftValue + maxHintWidth - targetHintWidth);
    NX_ASSERT(rect.width() >= minSize);

    qreal roundness = minSize / 2;
    QPainterPath path;
    path.addRoundedRect(rect, roundness, roundness);

    QBrush bgColor = m_state == Pressed
        ? colors.background
        : palette.window();

    painter->fillPath(path, bgColor);

    if (m_state == Pressed && qFuzzyEquals(m_hintVisibility, kVisible))
    {
        NX_ASSERT(m_stateTimer.isValid());
        QRectF visualizerRect(rect.adjusted(roundness, 0.0, -minSize, 0.0));
        NX_ASSERT(visualizerRect.isValid());
        if (!visualizerRect.isValid())
            return;

        qint64 oldTimeStamp = m_paintTimeStamp;
        m_paintTimeStamp = m_stateTimer.elapsed();
        qint64 timeStepMs = m_paintTimeStamp - oldTimeStamp;

        auto data = QnVoiceSpectrumAnalyzer::instance()->getSpectrumData().data;
        if (data.isEmpty())
        {
            paintVisualizer(painter, visualizerRect, generateEmptyVector(m_paintTimeStamp), colors.visualizer);
            return;
        }

        normalizeVector(data);

        /* Calculate size hint when first time receiving analyzed data. */
        if (m_visualizerData.isEmpty() && !data.isEmpty())
        {
            Q_Q(QnTwoWayAudioWidget);
            q->updateGeometry();
        }

        m_visualizerData = animateVector(m_visualizerData, data, timeStepMs);
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
            setHint(tr("Hold to Speak"));
            opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);

            Q_Q(QnTwoWayAudioWidget);
            QSizeF minSize = q->sizeHint(Qt::MinimumSize);
            QRectF widgetGeometry = q->geometry();
            widgetGeometry.adjust(widgetGeometry.width() - minSize.width(), 0, 0, 0);
            q->setGeometry(widgetGeometry);
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

    m_paintTimeStamp = 0;
    if (m_state == OK)
    {
        m_stateTimer.invalidate();
        m_hintTimer->stop();
    }
    else
    {
        m_stateTimer.restart();
        m_hintTimer->start();
    }

    ensureAnimator();
    disconnect(m_hintAnimator, nullptr, this, nullptr);
    m_hintAnimator->stop();
    if (open)
    {
        connect(m_hintAnimator, &AbstractAnimator::finished, this, [this]()
        {
            if (m_state == Released || m_state == Error)
                opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kVisible);
        });
        m_hintAnimator->setEasingCurve(QEasingCurve::OutQuad);
        m_hintAnimator->animateTo(kVisible);

    }
    else
    {
        m_hintAnimator->setEasingCurve(QEasingCurve::InQuad);
        m_hintAnimator->animateTo(kHidden);
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
    m_hintAnimator->setEasingCurve(QEasingCurve::InOutQuad);
    m_hintAnimator->setTimer(InstrumentManager::animationTimer(q->scene()));
    m_hintAnimator->setAccessor(accessor);
    m_hintAnimator->setTargetObject(this);
    m_hintAnimator->setTimeLimit(kShowHintAnimationTimeMs);
    m_hintAnimator->setSpeed(kShowHintAnimationSpeed);
}
