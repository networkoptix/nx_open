// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <nx/utils/impl_ptr.h>
#include <ui/animation/animated.h>

namespace nx::vms::client::desktop {

class RewindOverlay;

class RewindWidget: public Animated<QGraphicsWidget>
{
    Q_OBJECT
    using base_type = Animated<QGraphicsWidget>;

    static constexpr int kStopped = -1;

public:
    RewindWidget(QGraphicsItem* parent = nullptr, bool fastForward = true);
    ~RewindWidget();

    void updateSize(QSizeF size);

    void blink();
    void tick(int deltaMs);

private:
    virtual void paint(
        QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QGraphicsWidget* m_triangle1;
    QGraphicsWidget* m_triangle2;
    QGraphicsWidget* m_triangle3;
    QPixmap icon;

    QSizeF m_size;
    bool m_fastForward;

    class BackgroundWidget;
    BackgroundWidget* m_background;

    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();
    int m_totalMs = kStopped;
};

} // namespace nx::vms::client::desktop
