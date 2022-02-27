// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_button_p.h"

#include <cmath>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsDropShadowEffect>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/slider_tooltip_widget.h>
#include <ui/processors/hover_processor.h>
#include <nx/vms/client/desktop/style/icon.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

/* Duration of success and failure notifications staying up: */
static constexpr int kNotificationDurationMs = 1200;

/* Opacity for disabled button: */
static const qreal kDisabledTriggerOpacity = nx::style::Hints::kDisabledItemOpacity;

/* A short delay in State::Waiting before busy indicator appears: */
static constexpr int kBusyIndicatorDelayMs = 150;

/* Geometric adjustments: */
static const QSize kDefaultButtonSize(40, 40);
static constexpr qreal kFrameLineWidth = 2.0;
static constexpr qreal kBusyIndicatorDotRadius = 2.5;
static constexpr unsigned int kBusyIndicatorDotSpacing = 3;

/* Prolonged trigger frame pulsating opacity parameters: */
static constexpr int kAnimationPeriodMs = 1200;
static constexpr qreal kAnimationMinimumOpacity = 0.15;

/* Tooltip adjustments: */
static constexpr int kHoverEnterDelayMs = 0;
static constexpr int kHoverLeaveDelayMs = 25;
static constexpr qreal kToolTipFadeTimeMs = 25;
static constexpr qreal kToolTipRoundingRadius = 2.0;
static constexpr qreal kToolTipPadding = 8.0;
static constexpr qreal kToolTipShadowBlurRadius = 5.0;
static constexpr qreal kToolTipShadowOpacity = 0.36;
static const QPointF kToolTipShadowOffset(0.0, 3.0);

static Qt::Edge invertEdge(Qt::Edge edge)
{
    switch (edge)
    {
        case Qt::LeftEdge:
            return Qt::RightEdge;

        case Qt::RightEdge:
            return Qt::LeftEdge;

        case Qt::TopEdge:
            return Qt::BottomEdge;

        case Qt::BottomEdge:
            return Qt::TopEdge;

        default:
            NX_ASSERT(false);
            return Qt::LeftEdge;
    }
}

} // namespace

namespace nx::vms::client::desktop {

SoftwareTriggerButtonPrivate::SoftwareTriggerButtonPrivate(SoftwareTriggerButton* main):
    base_type(nullptr),
    q_ptr(main),
    m_toolTip(new QnSliderTooltipWidget(main)),
    m_toolTipHoverProcessor(new HoverFocusProcessor(main))
{
    setButtonSize(kDefaultButtonSize);

    connect(main, &SoftwareTriggerButton::geometryChanged,
        this, &SoftwareTriggerButtonPrivate::updateToolTipPosition);

    connect(main, &SoftwareTriggerButton::pressed, this,
        [this]() { setState(SoftwareTriggerButton::State::Default); });

    installEventHandler(main, QEvent::PaletteChange, this,
        [this]()
        {
            Q_Q(SoftwareTriggerButton);
            m_imagesDirty = true;
            q->update();
        });

    m_toolTipHoverProcessor->addTargetItem(main);
    m_toolTipHoverProcessor->setHoverEnterDelay(kHoverEnterDelayMs);
    m_toolTipHoverProcessor->setHoverLeaveDelay(kHoverLeaveDelayMs);

    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverEntered,
        this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverLeft,
        this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);

    m_toolTip->setOpacity(0.0);
    m_toolTip->setRoundingRadius(kToolTipRoundingRadius);
    m_toolTip->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);

    auto shadowEffect = new QGraphicsDropShadowEffect(m_toolTip);
    auto shadowColor = QPalette().color(QPalette::Shadow);
    shadowColor.setAlphaF(kToolTipShadowOpacity);
    shadowEffect->setColor(shadowColor);
    shadowEffect->setOffset(kToolTipShadowOffset);
    shadowEffect->setBlurRadius(kToolTipShadowBlurRadius);
    m_toolTip->setGraphicsEffect(shadowEffect);

    updateToolTipTailEdge();
}

SoftwareTriggerButtonPrivate::~SoftwareTriggerButtonPrivate()
{
    cancelScheduledChange();
}

QString SoftwareTriggerButtonPrivate::toolTip() const
{
    return m_toolTip->text();
}

void SoftwareTriggerButtonPrivate::setToolTip(const QString& toolTip)
{
    if (m_toolTipText == toolTip)
        return;

    m_toolTipText = toolTip;
    updateToolTipText();
}

void SoftwareTriggerButtonPrivate::updateToolTipText()
{
    m_toolTip->setText(m_live
        ? m_toolTipText
        : SoftwareTriggerButton::tr("Go to Live"));

    updateToolTipPosition();
}

Qt::Edge SoftwareTriggerButtonPrivate::toolTipEdge() const
{
    return m_toolTipEdge;
}

void SoftwareTriggerButtonPrivate::setToolTipEdge(Qt::Edge edge)
{
    if (m_toolTipEdge == edge)
        return;

    m_toolTipEdge = edge;
    updateToolTipTailEdge();
}

QSize SoftwareTriggerButtonPrivate::buttonSize() const
{
    return m_buttonSize;
}

void SoftwareTriggerButtonPrivate::setButtonSize(const QSize& size)
{
    if (m_buttonSize == size)
        return;

    m_buttonSize = size;
    m_imagesDirty = true;

    Q_Q(SoftwareTriggerButton);
    q->setFixedSize(m_buttonSize);
}

void SoftwareTriggerButtonPrivate::setIcon(const QString& name)
{
    const auto iconName = SoftwareTriggerPixmaps::effectivePixmapName(name);
    if (m_iconName == iconName)
        return;

    m_iconName = iconName;
    m_imagesDirty = true;
}

bool SoftwareTriggerButtonPrivate::prolonged() const
{
    return m_prolonged;
}

void SoftwareTriggerButtonPrivate::setProlonged(bool value)
{
    if (m_prolonged == value)
        return;

    m_prolonged = value;
    m_imagesDirty = true;

    Q_Q(SoftwareTriggerButton);
    if (m_prolonged)
    {
        connect(q, &SoftwareTriggerButton::pressed,
            this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
        connect(q, &SoftwareTriggerButton::released,
            this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
    }
    else
    {
        disconnect(q, &SoftwareTriggerButton::pressed,
            this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
        disconnect(q, &SoftwareTriggerButton::released,
            this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
    }
}

void SoftwareTriggerButtonPrivate::updateToolTipVisibility()
{
    static constexpr qreal kTransparent = 0.0;
    static constexpr qreal kOpaque = 1.0;

    Q_Q(const SoftwareTriggerButton);

    const bool showToolTip = m_toolTipHoverProcessor->isHovered()
        && !m_toolTip->text().isEmpty()
        && !(m_prolonged && q->isPressed());

    static constexpr qreal kToolTipAnimationSpeedFactor = 1000.0 / kToolTipFadeTimeMs;

    const qreal targetOpacity = showToolTip ? kOpaque : kTransparent;
    opacityAnimator(m_toolTip, kToolTipAnimationSpeedFactor)->animateTo(targetOpacity);
}

void SoftwareTriggerButtonPrivate::updateToolTipTailEdge()
{
    m_toolTip->setTailEdge(invertEdge(m_toolTipEdge));
    updateToolTipPosition();
}

void SoftwareTriggerButtonPrivate::updateToolTipPosition()
{
    Q_Q(const SoftwareTriggerButton);
    const auto size = q->size();

    switch (m_toolTipEdge)
    {
        case Qt::LeftEdge:
            m_toolTip->pointTo({ -kToolTipPadding, size.height() / 2 });
            break;

        case Qt::RightEdge:
            m_toolTip->pointTo({ size.width() + kToolTipPadding - 1, size.height() / 2 });
            break;

        case Qt::TopEdge:
            m_toolTip->pointTo({ size.width() / 2, -kToolTipPadding });
            break;

        case Qt::BottomEdge:
            m_toolTip->pointTo({ size.width() / 2, size.height() + kToolTipPadding - 1 });
            break;

        default:
            NX_ASSERT(false);
    }
}

bool SoftwareTriggerButtonPrivate::isLive() const
{
    return m_live;
}

bool SoftwareTriggerButtonPrivate::setLive(bool value)
{
    if (m_live == value)
        return false;

    m_live = value;
    updateToolTipText();

    Q_Q(SoftwareTriggerButton);
    q->update();

    return true;
}

SoftwareTriggerButton::State SoftwareTriggerButtonPrivate::state() const
{
    return m_state;
}

void SoftwareTriggerButtonPrivate::setState(SoftwareTriggerButton::State state)
{
    if (m_state == state)
        return;

    m_state = state;

    cancelScheduledChange();
    m_busyIndicator.reset(nullptr);

    const auto setNormalState =
        [this]()
        {
            setState(SoftwareTriggerButton::State::Default);
        };

    Q_Q(SoftwareTriggerButton);
    q->update();

    switch (m_state)
    {
        case SoftwareTriggerButton::State::Default:
            break;

        case SoftwareTriggerButton::State::Waiting:
        {
            if (m_prolonged)
            {
                /* Immediately reset to normal for released prolonged triggers: */
                if (!q->isPressed())
                    m_state = SoftwareTriggerButton::State::Default;

                /* Don't show busy indicator for prolonged triggers. */
                break;
            }

            const auto showBusyIndicator =
                [this]()
                {
                    Q_Q(SoftwareTriggerButton);
                    m_busyIndicator.reset(new BusyIndicatorGraphicsWidget(q));
                    m_busyIndicator->setPreferredSize(q->size());
                    m_busyIndicator->dots()->setDotRadius(kBusyIndicatorDotRadius);
                    m_busyIndicator->dots()->setDotSpacing(kBusyIndicatorDotSpacing);
                };

            scheduleChange(showBusyIndicator, kBusyIndicatorDelayMs);
            break;
        }

        case SoftwareTriggerButton::State::Success:
        {
            if (m_prolonged)
            {
                /* Immediately reset to normal for prolonged triggers: */
                m_state = SoftwareTriggerButton::State::Default;
                break;
            }

            scheduleChange(setNormalState, kNotificationDurationMs);
            break;
        }

        case SoftwareTriggerButton::State::Failure:
        {
            if (m_prolonged)
            {
                if (q->isPressed())
                {
                    /* Release pressed prolonged trigger in case of error: */
                    q->ungrabMouse();
                }
                else
                {
                    /* Restart animation timer: */
                    m_animationTime.start();

                    /* Don't do timed reset to normal for released prolonged triggers: */
                    break;
                }
            }

            scheduleChange(setNormalState, kNotificationDurationMs);
            break;
        }

        default:
        {
            NX_ASSERT(false); // should never get here
            break;
        }
    }

    /* Restart animation timer for active prolonged trigger: */
    if (m_prolonged && q->isPressed() && m_state == SoftwareTriggerButton::State::Default)
        m_animationTime.start();
}

void SoftwareTriggerButtonPrivate::scheduleChange(std::function<void()> callback, int delayMs)
{
    if (m_scheduledChangeTimer)
    {
        m_scheduledChangeTimer->stop();
        m_scheduledChangeTimer->deleteLater();
    }

    if (callback)
        m_scheduledChangeTimer = executeDelayedParented(callback, delayMs, this);
}

void SoftwareTriggerButtonPrivate::cancelScheduledChange()
{
    scheduleChange(std::function<void()>(), 0);
}

void SoftwareTriggerButtonPrivate::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    ensureImages();

    Q_Q(SoftwareTriggerButton);
    if (!m_live && (q->isHovered() || q->isPressed()))
    {
        paintPixmapSharp(painter, q->isPressed()
            ? m_goToLivePixmapPressed
            : m_goToLivePixmap);
        return;
    }

    const auto effectiveState =
        m_prolonged && q->isPressed() && m_state != SoftwareTriggerButton::State::Failure
            ? SoftwareTriggerButton::State::Default
            : m_state;

    const auto currentOpacity =
        [this]() -> qreal
        {
            static const qreal kTwoPi = 3.1415926 * 2.0;
            const auto angle = m_animationTime.elapsed() * kTwoPi / kAnimationPeriodMs;
            const auto value = (cos(angle) + 1.0) / 2.0; // [0...1]
            return kAnimationMinimumOpacity + value * (1.0 - kAnimationMinimumOpacity);
        };

    QnScopedPainterOpacityRollback opacityRollback(painter);

    if (q->isDisabled() || !m_live)
        painter->setOpacity(painter->opacity() * kDisabledTriggerOpacity);

    switch (effectiveState)
    {
        case SoftwareTriggerButton::State::Default:
        {
            q->SoftwareTriggerButton::base_type::paint(painter, option, widget);
            if (m_prolonged && q->isPressed())
            {
                painter->setOpacity(painter->opacity() * currentOpacity());
                paintPixmapSharp(painter, m_activityFramePixmap);
            }
            break;
        }

        case SoftwareTriggerButton::State::Waiting:
        {
            paintPixmapSharp(painter, m_waitingPixmap);
            break;
        }

        case SoftwareTriggerButton::State::Success:
        {
            paintPixmapSharp(painter, m_successPixmap); //< already with frame
            break;
        }

        case SoftwareTriggerButton::State::Failure:
        {
            paintPixmapSharp(painter, m_failurePixmap);
            if (m_prolonged && !q->isPressed())
                painter->setOpacity(painter->opacity() * currentOpacity());

            paintPixmapSharp(painter, m_failureFramePixmap);
            break;
        }

        default:
        {
            NX_ASSERT(false); // should never get here
            break;
        }
    }
}

QPixmap SoftwareTriggerButtonPrivate::generatePixmap(
    const QColor& background,
    const QColor& frame,
    const QPixmap& icon)
{
    const auto pixelRatio = qApp->devicePixelRatio();

    QPixmap target(m_buttonSize * pixelRatio);
    target.setDevicePixelRatio(pixelRatio);
    target.fill(Qt::transparent);

    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(background.isValid() ? QBrush(background) : QBrush(Qt::NoBrush));
    painter.setPen(frame.isValid() ? QPen(frame, kFrameLineWidth) : QPen(Qt::NoPen));

    painter.drawEllipse(frame.isValid()
        ? nx::vms::client::core::Geometry::eroded(QRectF(buttonRect()), kFrameLineWidth / 2.0)
        : buttonRect());

    if (!icon.isNull())
    {
        const auto pixmapRect = nx::vms::client::core::Geometry::aligned(
            icon.size() / icon.devicePixelRatio(),
            buttonRect());

        painter.drawPixmap(pixmapRect, icon);
    }

    return target;
}

void SoftwareTriggerButtonPrivate::ensureImages()
{
    if (!m_imagesDirty)
        return;

    m_imagesDirty = false;

    Q_Q(SoftwareTriggerButton);

    if (m_iconName.isEmpty())
    {
        q->setIcon(QIcon());
        return;
    }

    const auto buttonPixmap = SoftwareTriggerPixmaps::pixmapByName(m_iconName);

    const auto generateStatePixmap =
        [this, buttonPixmap](const QColor& backgroundColor) -> QPixmap
        {
            Q_Q(const SoftwareTriggerButton);
            return generatePixmap(backgroundColor, QColor(), buttonPixmap);
        };

    static const QColor kNormalColor = colorTheme()->color("dark1", 128);
    static const QColor kActiveColor = colorTheme()->color("brand_core", 179);
    static const QColor kPressedColor = colorTheme()->color("brand_core", 128);
    static const QColor kErrorBackgroundColor = colorTheme()->color("red_core", 128);
    static const QColor kErrorFrameColor = colorTheme()->color("red_core");

    QIcon icon;
    const auto normalPixmap = generateStatePixmap(kNormalColor);
    icon.addPixmap(normalPixmap, QIcon::Normal);
    icon.addPixmap(normalPixmap, QIcon::Disabled);
    icon.addPixmap(generateStatePixmap(kActiveColor), QIcon::Active);
    icon.addPixmap(generateStatePixmap(kPressedColor), QnIcon::Pressed);

    q->setIcon(icon);

    m_waitingPixmap = generatePixmap(
        kPressedColor,
        QColor(),
        QPixmap());

    m_successPixmap = generatePixmap(
        kPressedColor,
        q->palette().color(QPalette::Text),
        qnSkin->pixmap("soft_triggers/confirmation_success.png"));

    m_failurePixmap = generatePixmap(
        kErrorBackgroundColor,
        QColor(),
        qnSkin->pixmap("soft_triggers/confirmation_failure.png"));

    m_failureFramePixmap = generatePixmap(
        QColor(),
        kErrorFrameColor,
        QPixmap());

    m_activityFramePixmap = generatePixmap(
        QColor(),
        q->palette().color(QPalette::Text),
        QPixmap());

    const auto goToLivePixmap = qnSkin->pixmap("soft_triggers/go_to_live.png");

    m_goToLivePixmap = generatePixmap(
        kActiveColor,
        QColor(),
        goToLivePixmap);

    m_goToLivePixmapPressed = generatePixmap(
        kPressedColor,
        QColor(),
        goToLivePixmap);
}

QRect SoftwareTriggerButtonPrivate::buttonRect() const
{
    return QRect(QPoint(), m_buttonSize);
}

} // namespace nx::vms::client::desktop
