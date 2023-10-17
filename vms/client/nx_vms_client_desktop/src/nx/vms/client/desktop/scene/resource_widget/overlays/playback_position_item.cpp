// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "playback_position_item.h"

#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <qt_graphics_items/graphics_label.h>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/painter_transform_scale_stripper.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/playback_position_icon_text_widget.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/controls/html_text_item.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

PlaybackPositionItem::PlaybackPositionItem(WindowContext* windowContext, QGraphicsItem* parent):
    base_type(parent),
    WindowContextAware(windowContext),
    m_pauseButton(new PlaybackPositionIconTextWidget()),
    m_positionAndRecording(new PlaybackPositionIconTextWidget())
{
    static const QColor kPauseBackgroundColor = core::colorTheme()->color("timeline.playback.pause");
    static const QColor kArchiveBackgroundColor = core::colorTheme()->color("timeline.playback.archive");

    setAcceptedMouseButtons(Qt::NoButton);

    PlaybackPositionIconTextWidgetOptions pauseOptions;
    pauseOptions.backgroundColor = kPauseBackgroundColor;
    pauseOptions.borderRadius = 2;
    pauseOptions.vertPadding = 6;
    pauseOptions.horPadding = 7;
    pauseOptions.fixedHeight = 22;

    auto pauseIcon = qnSkin->icon("item/pause_button.svg");
    m_pauseButton->setParent(this);
    m_pauseButton->setVisible(false);
    m_pauseButton->setAcceptedMouseButtons(Qt::NoButton);
    m_pauseButton->setObjectName("PauseButton");
    m_pauseButton->setToolTip(tr("video is paused"));
    m_pauseButton->setOptions(pauseOptions);
    auto pausePixmap = pauseIcon.pixmap(QSize(8, 10));
    m_pauseButton->setIcon(pausePixmap);

    PlaybackPositionIconTextWidgetOptions positionOptions;
    positionOptions.backgroundColor = kArchiveBackgroundColor;
    positionOptions.horSpacing = 4;
    positionOptions.horPadding = 6;
    positionOptions.vertPadding = 4;
    positionOptions.borderRadius = 2;
    positionOptions.fixedHeight = 22;

    m_positionAndRecording->setVisible(true);
    m_positionAndRecording->setParent(this);
    m_positionAndRecording->setObjectName("IconButton");
    m_positionAndRecording->setOptions(positionOptions);

    auto mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    const auto ratio = qApp->devicePixelRatio();
    mainLayout->setContentsMargins(0, 0, 8 / ratio, 8 / ratio);
    mainLayout->addItem(m_pauseButton);
    mainLayout->addItem(m_positionAndRecording);

    mainLayout->setItemSpacing(0, 4);

    setLayout(mainLayout);

    connect(display()->playbackPositionBlinkTimer(), &QTimer::timeout, this,
        [this]()
        {
            if (qFuzzyIsNull(opacity()) && !m_isLive)
                blink();
        });

    connect(m_animationTimerListener.get(), &AnimationTimerListener::tick, this,
        &PlaybackPositionItem::tick);

    registerAnimation(m_animationTimerListener);
    m_animationTimerListener->startListening();

    connect(navigator(), &QnWorkbenchNavigator::positionChanged,
        this, &PlaybackPositionItem::cancelAnimation);
}

PlaybackPositionItem::~PlaybackPositionItem()
{
}

void PlaybackPositionItem::cancelAnimation()
{
    // Animation is not running
    if (m_totalMs == kStopped)
        return;

    m_totalMs = kStopped;
    auto animator = opacityAnimator(this);
    if (animator && animator->isRunning())
        animator->stop();
    this->setOpacity(0.0);
}

/*
 * Animate item blinking:
 * 1) show variant 2 (same as item shown when it is visible)
 *     - opacity of background 0.6 on pause and 0.5 on playing archive
 *     - opacity of entire element 1.0
 *    300ms animation time and hold for 400ms.
 * 2) repeat 3 times
 * 2a) show variant 3
 *     - opacity of background is 1.0
 *     - opacity of entire element is 0.8
 *    100ms animation time and hold for 400ms.
 * 2b) show variant 2
 *    100ms animation time and hold for 400ms,
 * 3) hide item
 *     - opacity of element to 0
 *    300ms animation time.
 */
void PlaybackPositionItem::tick(int deltaMs)
{
    static const QColor kPauseBackgroundColor = core::colorTheme()->color("timeline.playback.pause");
    static const QColor kArchiveBackgroundColor = core::colorTheme()->color("timeline.playback.archive");

    if (m_totalMs == kStopped)
        return;

    auto animator = opacityAnimator(this);
    if (!animator || animator->isRunning())
        return;

    m_totalMs += deltaMs;
    if (m_totalMs < 300)
    {
        animator->setTimeLimit(300);
        animator->animateTo(1.0);
        return;
    }

    // Animate from variant2 to variant3.
    if ((m_totalMs > 700 && m_totalMs <= 800) || (m_totalMs > 1700 && m_totalMs <= 1800)
        || (m_totalMs > 2700 && m_totalMs <= 2800))
    {
        animator->setTimeLimit(100);
        animator->animateTo(0.8);
        m_pauseButton->options().backgroundColor.setAlpha(255);
        m_positionAndRecording->options().backgroundColor.setAlpha(255);
        return;
    }

    // Animate from variant3 to variant2.
    if ((m_totalMs > 1200 && m_totalMs <= 1300) || (m_totalMs > 2200 && m_totalMs <= 2300)
        || (m_totalMs > 3200 && m_totalMs <= 3300))
    {
        animator->setTimeLimit(100);
        animator->animateTo(1.0);
        m_pauseButton->options().backgroundColor.setAlpha(kPauseBackgroundColor.alpha());
        m_positionAndRecording->options().backgroundColor.setAlpha(
            m_pauseButton->isVisible()
                ? kPauseBackgroundColor.alpha()
                : kArchiveBackgroundColor.alpha());
        return;
    }

    if (m_totalMs >= 3700)
    {
        m_totalMs = kStopped;
        animator->setTimeLimit(300);
        animator->animateTo(0.0);
        m_pauseButton->options().backgroundColor.setAlpha(kPauseBackgroundColor.alpha());
        m_positionAndRecording->options().backgroundColor.setAlpha(
            m_pauseButton->isVisible()
                ? kPauseBackgroundColor.alpha()
                : kArchiveBackgroundColor.alpha());
    }
}

void PlaybackPositionItem::blink()
{
    m_totalMs = 0;
}

void PlaybackPositionItem::setVisibleButtons(int buttons)
{
    static const QColor kPauseBackgroundColor = core::colorTheme()->color("timeline.playback.pause");
    static const QColor kArchiveBackgroundColor = core::colorTheme()->color("timeline.playback.archive");

    if (buttons & Qn::PauseButton)
    {
        m_pauseButton->setVisible(true);
        setRecordingIcon({});
        auto options = m_positionAndRecording->options();
        options.backgroundColor = kPauseBackgroundColor;
        m_positionAndRecording->setOptions(options);
    }
    else
    {
        m_pauseButton->setVisible(false);
        auto options = m_positionAndRecording->options();
        options.backgroundColor = kArchiveBackgroundColor;
        m_positionAndRecording->setOptions(options);
    }

    if (!(buttons & Qn::RecordingStatusIconButton))
        setRecordingIcon({});
}

int PlaybackPositionItem::visibleButtons()
{
    int buttons = isVisible() ? Qn::RecordingStatusIconButton : 0;
    if (m_pauseButton->isVisible())
        buttons = buttons & Qn::PauseButton;
    return buttons;
}

void PlaybackPositionItem::setPositionText(const QString& text)
{
    if (!m_positionAndRecording)
        return;

    m_positionAndRecording->setText(text);
}

void PlaybackPositionItem::setRecordingIcon(const QPixmap& icon)
{
    if (!m_positionAndRecording)
        return;

    m_positionAndRecording->setIcon(icon);

    const bool isLive = !icon.size().isEmpty();

    if (m_isLive == isLive)
        return;

    m_isLive = isLive;
    if (m_isLive)
        m_positionAndRecording->setVisible(true);
}

} // namespace nx::vms::client::desktop
