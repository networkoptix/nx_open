#include "two_way_audio_widget_p.h"

#include <chrono>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <ui/animation/variant_animator.h>
#include <ui/animation/opacity_animator.h>
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

#include <nx/vms/client/desktop/common/utils/accessor.h>

using namespace std::chrono;
using namespace nx::vms::client::desktop;

namespace {

/** If mouse button released before this timeout, show hint. */
constexpr int kHoldTimeoutMs = 500;

/** How fast hint should be displayed at maximum. */
constexpr int kShowHintAnimationTimeMs = 500;

/** Animation speed. 1.0 means fully expand for 1 second. */
constexpr qreal kShowHintAnimationSpeed = 2.0;

/** Animation speed. 1.0 means show/hide for 1 second. */
constexpr qreal kHintOpacityAnimationSpeed = 3.0;

/** How opaque should enabled button be painted. */
constexpr qreal kEnabledOpacityCoeff = 1.0;

/** How opaque should disabled button be painted. */
constexpr qreal kDisabledOpacityCoeff = 0.3;

/** Label font size. */
constexpr int kHintFontPixelSize = 14;

/** Max visualizer value change per second (on increasing and decreasing values). */
constexpr qreal kVisualizerAnimationUpSpeed = 5.0;
constexpr qreal kVisualizerAnimationDownSpeed = 1.0;

/** Maximum visualizer height. */
constexpr qreal kMaxVisualizerHeightCoeff = 0.9;

/** Values lower than that value will be counted as silence. */
constexpr qreal kNormalizerSilenceValue = 0.1;

/** Maximum value, to which all values are normalized. */
constexpr qreal kNormalizerIncreaseValue = 0.9;

/** Recommended visualizer line width. */
constexpr qreal kVisualizerLineWidth = 4.0;

/** Visualizer offset between lines. */
constexpr qreal kVisualizerLineOffset = 2.0;

/** How long hint should be displayed. */
constexpr int kShowHintMs = 2000;

/** How long error should be displayed. */
constexpr int kShowErrorMs = 2000;

/** How fast should we warn user about absent data. */
constexpr int kDataTimeoutMs = 2000;

constexpr qreal kHidden = 0.0;
constexpr qreal kVisible = 1.0;

void paintVisualizer(
    QPainter* painter, const QRectF& rect, const VisualizerData& data, const QColor& color)
{
    if (data.isEmpty())
        return;

    const qreal lineWidth = qRound(qMax(kVisualizerLineWidth,
        (rect.width() / data.size()) - kVisualizerLineOffset));

    const qreal midY = rect.center().y();
    const qreal maxHeight = rect.height() * kMaxVisualizerHeightCoeff;

    QPainterPath path;
    for (int i = 0; i < data.size(); ++i)
    {
        const qreal lineHeight = qRound(qMax(maxHeight * data[i], kVisualizerLineOffset * 2));
        path.addRect(qRound(rect.left() + i * (lineWidth + kVisualizerLineOffset)),
            qRound(midY - (lineHeight / 2)), lineWidth, lineHeight);
    }

    paintSharp(painter,
        [&path, &color](QPainter* painter) { painter->fillPath(path, color); });
}

void normalizeData(VisualizerData& source)
{
    if (source.isEmpty())
        return;

    const auto max = std::max_element(source.cbegin(), source.cend());

    /* Do not normalize if silence */
    if (*max < kNormalizerSilenceValue)
        return;

    /* Do not normalize if there is bigger value, so normalizing will always only increase values. */
    if (*max > kNormalizerIncreaseValue)
        return;

    const auto k = kNormalizerIncreaseValue / *max;
    for (auto& e: source)
        e *= k;
}

VisualizerData animateData(const VisualizerData& prev, const VisualizerData& next, qint64 timeStepMs)
{
    //NX_ASSERT(next.size() == QnVoiceSpectrumAnalyzer::bandsCount());

    if (prev.size() != next.size())
        return next;

    const qreal maxUpChange = qBound(0.0, kVisualizerAnimationUpSpeed * timeStepMs / 1000, 1.0);
    const qreal maxDownChange = qBound(0.0, kVisualizerAnimationDownSpeed * timeStepMs / 1000, 1.0);

    VisualizerData result(prev.size());
    for (int i = 0; i < prev.size(); ++i)
    {
        const auto current = prev[i];
        const auto target = next[i];
        auto change = target - current;
        if (change > 0)
            change = qMin(change, maxUpChange);
        else
            change = qMax(change, -maxDownChange);
        result[i] = qBound(0.0, current + change, 1.0);
    }

    return result;
}

VisualizerData generateEmptyData(qint64 elapsedMs)
{
    // Making slider move forth and back...
    const int size = QnVoiceSpectrumAnalyzer::bandsCount();
    const int maxIdx = size * 2 - 1;

    VisualizerData result(QnVoiceSpectrumAnalyzer::bandsCount(), 0.0);
    int idx = qRound(16.0 * elapsedMs / 1000) % maxIdx;
    if (idx >= size)
        idx = maxIdx - idx;

    const bool isValidIndex = idx >= 0 && idx < result.size();
    NX_ASSERT(isValidIndex, "Invalid timeStep value");
    if (isValidIndex)
        result[idx] = 0.2;

    return result;
}

} // namespace

QnTwoWayAudioWidget::Private::Private(
    const QString& sourceId,
    QnTwoWayAudioWidget* owner)
    :
    base_type(),
    button(new QnImageButtonWidget(owner)),
    hint(new GraphicsLabel(owner)),
    q(owner),
    m_sourceId(sourceId),
    m_hintTimer(new QTimer(this))
{
    hint->setAcceptedMouseButtons(0);
    hint->setPerformanceHint(GraphicsLabel::PixmapCaching);
    hint->setAlignment(Qt::AlignCenter);
    QFont f = hint->font();
    f.setBold(true);
    f.setPixelSize(kHintFontPixelSize);
    hint->setFont(f);
    hint->setOpacity(kHidden);

    button->setIcon(qnSkin->icon("soft_triggers/user_selectable/mic.png"));
    button->setCheckable(false);

    connect(button, &QnImageButtonWidget::pressed, this,
        [this]()
        {
            startStreaming();
            emit q->pressed();
        });

    connect(button, &QnImageButtonWidget::released,
        this, &QnTwoWayAudioWidget::Private::stopStreaming);

    m_hintTimer->setInterval(100ms);
    m_hintTimer->setSingleShot(false);
    connect(m_hintTimer, &QTimer::timeout, this, [this]
    {
        if (!m_stateTimer.isValid())
            return;

        int timeout = 0;
        switch (m_state)
        {
            case HintState::OK:
                return;

            case HintState::Pressed:
                if (m_stateTimer.hasExpired(kDataTimeoutMs))
                {
                    auto data = QnVoiceSpectrumAnalyzer::instance()->getSpectrumData().data;
                    if (data.isEmpty())
                    {
                        setHint(tr("Input device is not selected"));
                        setState(HintState::Error);
                        stopStreaming();
                    }
                }
                return;

            case HintState::Released:
                timeout = kShowHintMs;
                break;

            case HintState::Error:
                timeout = kShowErrorMs;
                break;
        }

        if (m_stateTimer.hasExpired(timeout))
            setState(HintState::OK);
    });
}

QnTwoWayAudioWidget::Private::~Private()
{
}

void QnTwoWayAudioWidget::Private::updateCamera(const QnVirtualCameraResourcePtr& camera)
{
    NX_ASSERT(!m_camera && camera);

    if (m_camera)
        m_camera->disconnect(this);

    m_camera = camera;

    // If the given camera is I/O Module, then the license is required.
    if (camera && m_camera->isIOModule())
        m_licenseHelper.reset(new QnSingleCamLicenseStatusHelper(m_camera));
    else
        m_licenseHelper.reset();

    const auto updateState =
        [this]()
        {
            const bool enabled = isAllowed();
            q->setEnabled(enabled);
            button->setEnabled(enabled);
            if (!enabled)
                stopStreaming();

            q->setOpacity(enabled ? kEnabledOpacityCoeff : kDisabledOpacityCoeff);
        };

    if (camera)
        connect(camera.get(), &QnResource::statusChanged, this, updateState);

    if (m_licenseHelper)
    {
        connect(m_licenseHelper, &QnSingleCamLicenseStatusHelper::licenseStatusChanged, this,
            updateState);
    }

    updateState();
}

bool QnTwoWayAudioWidget::Private::isAllowed() const
{
    if (!m_camera || !m_camera->isOnline())
        return false;

    // Check if we are require licenses for two-way audio.
    if (m_licenseHelper)
        return m_licenseHelper->status() == QnLicenseUsageStatus::used;

    return true;
}

void QnTwoWayAudioWidget::Private::startStreaming()
{
    if (!isAllowed() || !m_camera || m_started)
        return;

    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(
        commonModule()->remoteGUID());

    if (!server || server->getStatus() != Qn::Online)
        return;

    setState(HintState::Pressed);

    m_requestHandle = server->restConnection()->twoWayAudioCommand(m_sourceId,
        m_camera->getId(),
        true,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (handle != m_requestHandle)
                return;

            if (m_state != HintState::Pressed)
                return;

            if (!success || result.error != QnRestResult::NoError)
            {
                setHint(tr("Streaming is not ready yet"));
                setState(HintState::Error);
                stopStreaming();
            }

        }, QThread::currentThread());

    m_started = m_requestHandle > 0;
    if (!m_started)
    {
        setHint(tr("Network error"));
        setState(HintState::Error);
    }
}

void QnTwoWayAudioWidget::Private::stopStreaming()
{
    if (!m_started)
        return;

    NX_ASSERT(m_state == HintState::Pressed || m_state == HintState::Error, "Invalid state");
    if (m_state != HintState::Error)
        setState(HintState::Released);

    m_started = false;
    m_requestHandle = 0;
    QnVoiceSpectrumAnalyzer::instance()->reset();
    m_visualizerData = VisualizerData();

    if (!m_camera)
        return;

    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(
        commonModule()->remoteGUID());

    if (!server || server->getStatus() != Qn::Online)
        return;

    // TODO: #GDM What should we do if we cannot stop streaming?

    /* Sending stop anyway, because we can get here in 'Streaming is not ready' error state. */
    server->restConnection()->twoWayAudioCommand(m_sourceId, m_camera->getId(), false,
        rest::ServerConnection::GetCallback());
}

void QnTwoWayAudioWidget::Private::setFixedHeight(qreal height)
{
    const auto size = qnSkin->maximumSize(button->icon());
    const QPointF excess(qMax(height - size.width(), 0.0), qMax(height - size.height(), 0.0));
    button->setFixedSize(height);
    button->setImageMargins({excess.x() / 2, excess.y() / 2, excess.x() / 2, excess.y() / 2});

    hint->setMinimumHeight(height);
    hint->setMaximumHeight(height);

    const qreal leftMargin = height / 2;
    q->setContentsMargins(leftMargin, 0.0, 0.0, 0.0);
    q->updateGeometry();
}

void QnTwoWayAudioWidget::Private::paint(
    QPainter* painter, const QRectF& sourceRect, const QPalette& palette)
{
    const qreal minSize = button->geometry().width();
    const qreal minLeftValue = sourceRect.left(); //< Left value if hint is visible.
    const qreal maxLeftValue = sourceRect.width() - minSize; //< Left value if hint is hidden.

    const qreal maxHintWidth = (maxLeftValue - minLeftValue);
    const qreal targetHintWidth = maxHintWidth * m_hintVisibility;

    QRectF rect(sourceRect);
    rect.setLeft(minLeftValue + maxHintWidth - targetHintWidth);
    NX_ASSERT(rect.width() >= minSize);

    const qreal roundness = minSize / 2;
    QPainterPath path;
    path.addRoundedRect(rect, roundness, roundness);

    const auto background = m_state == HintState::Pressed
        ? colors.background
        : (q->isUnderMouse() ? palette.midlight() : palette.window());

    painter->fillPath(path, background);

    if (m_state == HintState::Pressed && qFuzzyEquals(m_hintVisibility, kVisible))
    {
        NX_ASSERT(m_stateTimer.isValid());
        const auto visualizerRect = rect.adjusted(roundness, 0.0, -minSize, 0.0);
        NX_ASSERT(visualizerRect.isValid());
        if (!visualizerRect.isValid())
            return;

        const auto oldTimeStamp = m_paintTimeStamp;
        m_paintTimeStamp = m_stateTimer.elapsed();
        const auto timeStepMs = m_paintTimeStamp - oldTimeStamp;

        auto data = QnVoiceSpectrumAnalyzer::instance()->getSpectrumData().data;
        if (data.isEmpty())
        {
            paintVisualizer(painter, visualizerRect, generateEmptyData(m_paintTimeStamp),
                colors.visualizer);
        }
        else
        {
            normalizeData(data);

            // Calculate size hint when first time receiving analyzed data.
            if (m_visualizerData.isEmpty() && !data.isEmpty())
                q->updateGeometry();

            m_visualizerData = animateData(m_visualizerData, data, timeStepMs);
            paintVisualizer(painter, visualizerRect, m_visualizerData, colors.visualizer);
        }
    }
}

void QnTwoWayAudioWidget::Private::setState(HintState state)
{
    if (m_state == state)
        return;

    m_state = state;

    bool open = false;
    switch (m_state)
    {
        case HintState::Pressed:
        {
            open = true;
            setHint(tr("Hold to Speak"));
            opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);

            const QSizeF minSize = q->sizeHint(Qt::MinimumSize);
            QRectF widgetGeometry = q->geometry();
            widgetGeometry.adjust(widgetGeometry.width() - minSize.width(), 0, 0, 0);
            q->setGeometry(widgetGeometry);
            break;
        }

        case HintState::Released:
            open = m_stateTimer.isValid() && !m_stateTimer.hasExpired(kHoldTimeoutMs);
            break;

        case HintState::Error:
            open = true;
            break;

        default:
            break;
    }

    m_paintTimeStamp = 0;
    if (m_state == HintState::OK)
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
    m_hintAnimator->disconnect(this);
    m_hintAnimator->stop();

    if (open)
    {
        connect(m_hintAnimator, &AbstractAnimator::finished, this,
            [this]()
            {
                if (m_state == HintState::Released || m_state == HintState::Error)
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

void QnTwoWayAudioWidget::Private::setHint(const QString& text)
{
    hint->setText(text);
    q->updateGeometry();
}

void QnTwoWayAudioWidget::Private::ensureAnimator()
{
    if (m_hintAnimator)
        return;

    const auto hintCoordGetter =
        [this](const QObject* /*target*/) -> qreal
        {
            return m_hintVisibility;
        };

    const auto hintCoordSetter =
        [this](const QObject* /*target*/, const QVariant& value)
        {
            m_hintVisibility = value.toDouble();
            q->update();
        };

    m_hintAnimator = new VariantAnimator(this);
    m_hintAnimator->setEasingCurve(QEasingCurve::InOutQuad);
    m_hintAnimator->setTimer(InstrumentManager::animationTimer(q->scene()));
    m_hintAnimator->setAccessor(newAccessor(hintCoordGetter, hintCoordSetter));
    m_hintAnimator->setTargetObject(this);
    m_hintAnimator->setTimeLimit(kShowHintAnimationTimeMs);
    m_hintAnimator->setSpeed(kShowHintAnimationSpeed);
}
