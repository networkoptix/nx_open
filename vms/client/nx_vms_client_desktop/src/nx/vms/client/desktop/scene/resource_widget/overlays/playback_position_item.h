// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <nx/vms/client/desktop/window_context_aware.h>
#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>

class GraphicsLabel;
class QnImageButtonWidget;
class QnHtmlTextItem;
class PlaybackPositionIconTextWidget;


namespace nx::vms::client::desktop {

class WindowContext;

class PlaybackPositionItem: public Animated<QGraphicsWidget>, WindowContextAware
{
    Q_OBJECT
    using base_type = Animated<QGraphicsWidget>;

    static constexpr int kStopped = -1;

public:
    PlaybackPositionItem(WindowContext* windowContext, QGraphicsItem* parent = nullptr);
    virtual ~PlaybackPositionItem() override;

    void setVisibleButtons(int buttons);
    int visibleButtons();
    void setRecordingIcon(const QPixmap& icon);
    void setPositionText(const QString& text);

    void blink();
    void tick(int deltaMs);

private:
    void cancelAnimation();

    PlaybackPositionIconTextWidget* m_pauseButton = nullptr;
    PlaybackPositionIconTextWidget* m_positionAndRecording = nullptr;

    bool m_isLive = false;
    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();
    int m_totalMs = kStopped;
};

}; // namespace nx::vms::client::desktop
