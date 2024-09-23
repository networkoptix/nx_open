// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_widget_p.h"

#include <chrono>

#include <qt_graphics_items/graphics_label.h>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/audio/audiodevice.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/license/usage_helper.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/two_way_audio_widget.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace std::chrono;
using namespace nx::vms::client;
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

/** How long hint should be displayed. */
constexpr int kShowHintMs = 2000;

/** How long error should be displayed. */
constexpr int kShowErrorMs = 2000;

/** How fast should we warn user about absent data. */
constexpr int kDataTimeoutMs = 2000;

constexpr qreal kHidden = 0.0;
constexpr qreal kVisible = 1.0;

} // namespace

QnTwoWayAudioWidget::Private::Private(
    const QString& sourceId,
    const QnSecurityCamResourcePtr& camera,
    QnTwoWayAudioWidget* owner)
    :
    base_type(),
    button(new QnImageButtonWidget(owner)),
    hint(new GraphicsLabel(owner)),
    q(owner),
    m_sourceId(sourceId),
    m_camera(camera),
    m_hintTimer(new QTimer(this))
{
    m_voiceSpectrumPainter.options.color = core::colorTheme()->color("camera.twoWayAudio.visualizer");

    m_controller = std::make_unique<nx::vms::client::core::TwoWayAudioController>(
        SystemContext::fromResource(m_camera));
    m_controller->setSourceId(sourceId);
    connect(m_controller.get(),
        &nx::vms::client::core::TwoWayAudioController::availabilityChanged,
        this,
        &Private::updateState);
    m_controller->setResourceId(m_camera->getId());

    connect(m_camera.get(), &QnResource::statusChanged, this, &Private::updateState);

    hint->setAcceptedMouseButtons(Qt::NoButton);
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

    connect(button, &QnImageButtonWidget::released, this,
        [this]()
        {
            stopStreaming();
            emit q->released();
        });

    m_hintTimer->setInterval(100ms);
    m_hintTimer->setSingleShot(false);
    connect(m_hintTimer, &QTimer::timeout, this, [this]
    {
        if (!m_stateTimer.isValid())
            return;

        int timeout = 0;
        switch (m_state)
        {
            case HintState::ok:
                return;

            case HintState::pressed:
                if (m_stateTimer.hasExpired(kDataTimeoutMs))
                {
                    auto data = appContext()->voiceSpectrumAnalyzer()->getSpectrumData().data;
                    if (data.isEmpty())
                    {
                        setHint(tr("Input device is not selected"));
                        setState(HintState::error);
                        stopStreaming();
                    }
                }
                return;

            case HintState::released:
                timeout = kShowHintMs;
                break;

            case HintState::error:
                timeout = kShowErrorMs;
                break;
        }

        if (m_stateTimer.hasExpired(timeout))
            setState(HintState::ok);
    });

    updateState();
}

QnTwoWayAudioWidget::Private::~Private()
{
}

void QnTwoWayAudioWidget::Private::updateState()
{
    const bool enabled = isAllowed();
    q->setEnabled(enabled);
    button->setEnabled(enabled);
    if (!enabled)
        stopStreaming();

    q->setOpacity(enabled ? kEnabledOpacityCoeff : kDisabledOpacityCoeff);
}

bool QnTwoWayAudioWidget::Private::isAllowed() const
{
    return m_controller->available();
}

void QnTwoWayAudioWidget::Private::startStreaming()
{
    if (!isAllowed() || !m_camera || m_controller->started())
        return;

    auto systemContext = SystemContext::fromResource(m_camera);
    if (!NX_ASSERT(systemContext))
        return;

    const auto server = systemContext->currentServer();
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
        return;

    setState(HintState::pressed);

    const auto requestCallback =
        [this](bool success)
        {
            if (success)
            {
                if (appContext()->localSettings()->muteOnAudioTransmit()
                    && !nx::audio::AudioDevice::instance()->isMute())
                {
                    m_unmuteAudioOnStreamingStop = true;
                    nx::audio::AudioDevice::instance()->setMute(true);
                }
                return;
            }

            if (m_state != HintState::pressed)
                return;

            setHint(tr("Streaming is not ready yet"));
            setState(HintState::error);
            stopStreaming();
        };

    if (!m_controller->start(requestCallback))
    {
        setHint(tr("Network error"));
        setState(HintState::error);
    }
}

void QnTwoWayAudioWidget::Private::stopStreaming()
{
    if (!m_controller->started())
        return;

    if (m_unmuteAudioOnStreamingStop)
    {
        m_unmuteAudioOnStreamingStop = false;
        nx::audio::AudioDevice::instance()->setMute(false);
    }

    NX_ASSERT(m_state == HintState::pressed || m_state == HintState::error, "Invalid state");
    if (m_state != HintState::error)
        setState(HintState::released);

    m_controller->stop();
    appContext()->voiceSpectrumAnalyzer()->reset();
    m_voiceSpectrumPainter.reset();
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

void QnTwoWayAudioWidget::Private::paint(QPainter* painter, const QRectF& sourceRect)
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

    const auto background = m_state == HintState::pressed
        ? core::colorTheme()->color("camera.twoWayAudio.background.pressed")
        : (q->isUnderMouse()
            ? core::colorTheme()->color("camera.twoWayAudio.background.hovered")
            : core::colorTheme()->color("camera.twoWayAudio.background.default"));

    painter->fillPath(path, background);

    if (m_state == HintState::pressed && qFuzzyEquals(m_hintVisibility, kVisible))
    {
        NX_ASSERT(m_stateTimer.isValid());
        const auto visualizerRect = rect.adjusted(roundness, 0.0, -minSize, 0.0);
        NX_ASSERT(visualizerRect.isValid());
        if (!visualizerRect.isValid())
            return;

        m_voiceSpectrumPainter.update(m_stateTimer.elapsed(), appContext()->voiceSpectrumAnalyzer()->getSpectrumData().data);
        m_voiceSpectrumPainter.paint(painter, visualizerRect);
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
        case HintState::pressed:
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

        case HintState::released:
            open = m_stateTimer.isValid() && !m_stateTimer.hasExpired(kHoldTimeoutMs);
            break;

        case HintState::error:
            open = true;
            break;

        default:
            break;
    }

    if (m_state == HintState::ok)
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
                if (m_state == HintState::released || m_state == HintState::error)
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
