// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/pending_operation.h>
#include <ui/animation/animation_timer.h>

class InstrumentPaintSyncer: public QObject, public AnimationTimer
{
public:
    InstrumentPaintSyncer(QObject* parent = nullptr);

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

    void setFpsLimit(int limit);

protected:
    virtual void activatedNotify() override;
    virtual void deactivatedNotify() override;

    QObject* currentWatched() const;

private:
    void tick(int deltaTime);

private:
    QtBasedAnimationTimer* m_animationTimer;
    QPointer<QObject> m_currentWatched;
    QWidget* m_currentWidget;
    bool m_sendPaintEvent = false;
    nx::utils::ImplPtr<nx::utils::PendingOperation> m_update;
    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();
};
