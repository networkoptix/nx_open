// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_button.h"

#include <QtCore/QTimer>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <qt_graphics_items/graphics_label.h>

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/media/voice_spectrum_analyzer.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/resource/voice_spectrum_painter.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

constexpr qreal kHidden = 0.0;
constexpr qreal kVisible = 1.0;

/** How fast should we warn user about absent data. */
constexpr int kDataTimeoutMs = 2000;

/** How fast hint should be displayed at maximum. */
constexpr int kShowHintAnimationTimeMs = 500;

/** Animation speed. 1.0 means fully expand for 1 second. */
constexpr qreal kShowHintAnimationSpeed = 2.0;

/** Label font size. */
constexpr int kHintFontPixelSize = 14;

/** How long error should be displayed. */
constexpr int kShowErrorMs = 2000;

/** How long hint should be displayed. */
constexpr int kShowHintMs = 2000;

/** Animation speed. 1.0 means show/hide for 1 second. */
constexpr qreal kHintOpacityAnimationSpeed = 3.0;

/** If mouse button released before this timeout, show hint. */
constexpr int kHoldTimeoutMs = 500;

enum class HintState
{
    ok, //< Hint is hidden.
    pressed, //< Just pressed, waiting to check.
    released, //< Button is released too fast, hint is required.
    error, //< Some error occurred.
};

} // namespace

struct TwoWayAudioButton::Private
{
    TwoWayAudioButton* const q;

    GraphicsLabel* const hint;
    QTimer hintStateTimer = QTimer{};
    QElapsedTimer stateTimer;
    VoiceSpectrumPainter voiceSpectrumPainter;
    HintState hintState = HintState::ok;
    VariantAnimator* hintAnimator = nullptr;
    qreal hintVisibility = 0.0; //< Visibility of the hint. 0 - hidden, 1 - displayed.

    void applyCurrentHintState();
    void rollOutHint();
    void rollInHint();
    void setupHint();
    void setHintState(HintState state);
    void setHint(const QString& text);
    void updateState();
    void ensureAnimator();
};

void TwoWayAudioButton::Private::applyCurrentHintState()
{
    if (hintState == HintState::pressed)
    {
        rollOutHint();
        setHint(TwoWayAudioButton::tr("Hold to Speak"));

        opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);
        if (stateTimer.isValid() && stateTimer.hasExpired(kDataTimeoutMs)
            && core::appContext()->voiceSpectrumAnalyzer()->getSpectrumData().data.isEmpty())
        {
            setHint(TwoWayAudioButton::tr("Input device is not selected"));
            setHintState(HintState::error);
        }
        return;
    }

    int hideTimeoutMs = 0;

    switch (hintState)
    {
        case HintState::released:
            if (stateTimer.isValid() && !stateTimer.hasExpired(kHoldTimeoutMs))
                setHintState(HintState::error);
            break;
        case HintState::error:
            hideTimeoutMs = kShowErrorMs;
            if (!hintAnimator->isRunning())
                opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kVisible);
            break;
        default:
            return;
    }

    if (stateTimer.isValid() && stateTimer.hasExpired(hideTimeoutMs)) // FIXMEY: chrono.
        rollInHint();
}

void TwoWayAudioButton::Private::rollOutHint()
{
    if (stateTimer.isValid())
        return;

    ensureAnimator();
    stateTimer.restart();
    hintStateTimer.start();
    hintAnimator->setEasingCurve(QEasingCurve::OutQuad);
    hintAnimator->animateTo(kVisible);
    core::appContext()->voiceSpectrumAnalyzer()->reset();
    opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);
}

void TwoWayAudioButton::Private::rollInHint()
{
    ensureAnimator();
    hintAnimator->setEasingCurve(QEasingCurve::InQuad);
    hintAnimator->animateTo(kHidden);
    opacityAnimator(hint, kHintOpacityAnimationSpeed)->animateTo(kHidden);

    setHintState(HintState::ok);

    const auto handleRollingInFinished =
        [this]()
        {
            stateTimer.invalidate();
            hintStateTimer.stop();
            hintAnimator->disconnect(q);

            if (q->state() == State::Failure)
                q->setState(State::Default);
        };

    if (hintAnimator->isRunning())
        connect(&*hintAnimator, &AbstractAnimator::finished, q, handleRollingInFinished);
    else
        handleRollingInFinished();
}

void TwoWayAudioButton::Private::setupHint()
{
    hint->setAcceptedMouseButtons(Qt::NoButton);
    hint->setPerformanceHint(GraphicsLabel::PixmapCaching);
    hint->setAlignment(Qt::AlignCenter);
    hint->setFont(
        [this]()
        {
            auto result = hint->font();
            result.setBold(true);
            result.setPixelSize(kHintFontPixelSize);
            return result;
        }());
    hint->setOpacity(kHidden/*kVisible*/);

    hintStateTimer.setInterval(100ms);
    hintStateTimer.setSingleShot(false);
    connect(&hintStateTimer, &QTimer::timeout, q,
        [this]()
        {
            if (stateTimer.isValid())
                applyCurrentHintState();
        });
}

void TwoWayAudioButton::Private::setHintState(HintState value)
{
    if (hintState == value)
        return;

    hintState = value;

    if (hintState == HintState::error)
    {
        if (stateTimer.isValid())
            stateTimer.restart(); //< Give some time to user to read the error before hint roll in.
        else
            rollOutHint();
    }

    applyCurrentHintState();
}

void TwoWayAudioButton::Private::setHint(const QString& text)
{
    hint->setText(text);
    hint->adjustSize();
    const auto& size = hint->geometry().size();
    const auto& topLeft = QPointF(-size.width(), (q->geometry().height() - size.height()) / 2);
    hint->setGeometry(QRectF(topLeft, size));
}

void TwoWayAudioButton::Private::updateState()
{
    switch (q->state())
    {
        case State::Default:
            if (hintState != HintState::error) //< Error state will be reset by itself.
                setHintState(HintState::ok);
            break;
        case State::WaitingForActivation:
            setHintState(HintState::pressed);
            break;
        case State::WaitingForDeactivation:
            setHintState(HintState::released);
            break;
        case State::Failure:
            setHint(TwoWayAudioButton::tr("Network error"));
            setHintState(HintState::error);
            break;
        default:
            break;
    }
}

void TwoWayAudioButton::Private::ensureAnimator()
{
    if (hintAnimator)
        return;

    const auto hintVisibilityGetter =
        [this](const QObject* /*target*/)
        {
            return hintVisibility;
        };

    const auto hintVisibilitySetter =
        [this](const QObject* /*target*/, const QVariant& value)
        {
            hintVisibility = value.toDouble();
            q->update();
        };

    hintAnimator = new VariantAnimator(q);
    hintAnimator->setEasingCurve(QEasingCurve::InOutQuad);
    hintAnimator->setTimer(InstrumentManager::animationTimer(q->scene()));
    hintAnimator->setAccessor(newAccessor(hintVisibilityGetter, hintVisibilitySetter));
    hintAnimator->setTargetObject(q);
    hintAnimator->setTimeLimit(kShowHintAnimationTimeMs);
    hintAnimator->setSpeed(kShowHintAnimationSpeed);
}

//-------------------------------------------------------------------------------------------------

TwoWayAudioButton::TwoWayAudioButton(QGraphicsItem* parent):
    base_type(parent, Qt::transparent, Qt::transparent, Qt::transparent),
    d(new Private{.q = this, .hint = new GraphicsLabel(this)})
{
    setPaletteColor(this, QPalette::Window,
        core::colorTheme()->color("camera.twoWayAudio.background.default"));
    setPaletteColor(this, QPalette::WindowText,
        core::colorTheme()->color("camera.twoWayAudio.text"));

    d->voiceSpectrumPainter.options.color =
        core::colorTheme()->color("camera.twoWayAudio.visualizer");

    d->setupHint();
    connect(this, &CameraButton::stateChanged, this, [this]() { d->updateState(); });
}

TwoWayAudioButton::~TwoWayAudioButton()
{
}

void TwoWayAudioButton::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
    QRectF rect(option->rect);
    const qreal spacing = buttonSize().width() / 2;
    const qreal hintWidth = d->hint->geometry().size().width() + spacing;
    const qreal targetHintWidth = hintWidth * d->hintVisibility;

    rect.setLeft(rect.left() - targetHintWidth);

    const qreal minWidth = geometry().width();
    const qreal roundness = minWidth / 2;
    QPainterPath path;
    path.addRoundedRect(rect, roundness, roundness);

    const auto background =
        [this]()
        {
            static constexpr int kColorBlinkTimeoutMs = 200;
            if (d->hintState == HintState::pressed && d->stateTimer.hasExpired(kColorBlinkTimeoutMs))
            {
                // Set correct color and aviod blink from pressed to
                return core::colorTheme()->color("camera.twoWayAudio.background.pressed");
            }

            return isUnderMouse() || d->hint->isUnderMouse()
                ? core::colorTheme()->color("camera.twoWayAudio.background.hovered")
                : core::colorTheme()->color("camera.twoWayAudio.background.default");
        }();

    painter->fillPath(path, background);

    if (d->hintState == HintState::pressed && qFuzzyEquals(d->hintVisibility, kVisible))
    {
        NX_ASSERT(d->stateTimer.isValid());
        const auto visualizerRect = rect.adjusted(roundness, 0.0, -minWidth, 0.0);
        if (!NX_ASSERT(visualizerRect.isValid()))
            return;

        d->voiceSpectrumPainter.update(d->stateTimer.elapsed(), core::appContext()->voiceSpectrumAnalyzer()->getSpectrumData().data);
        d->voiceSpectrumPainter.paint(painter, visualizerRect);
    }

    base_type::paint(painter, option, widget);
}

} // namespace nx::vms::client::desktop
