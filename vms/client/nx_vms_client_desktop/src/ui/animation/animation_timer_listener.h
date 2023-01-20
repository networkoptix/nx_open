// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

class AnimationTimer;

/**
 * Helper class for detached animation processing. Should be used as a shared pointer member. Set
 * onTick function to process the animation. It is to be used in conjunction with AnimationTimer.
 */
class AnimationTimerListener final:
    public QObject,
    public std::enable_shared_from_this<AnimationTimerListener>
{
    Q_OBJECT

public:
    static std::shared_ptr<AnimationTimerListener> create();

    virtual ~AnimationTimerListener();

    AnimationTimer* timer() const { return m_timer; }
    void setTimer(AnimationTimer* timer);

    /**
     * Activates the link between this listener and its associated timer so
     * that this listener starts receiving tick notifications.
     */
    void startListening();

    /**
     * Deactivates the link between this listener and its associated timer so
     * that this listener stops receiving tick notifications.
     */
    void stopListening();

    /**
     * Whether this listener is open for receiving tick notifications.
     */
    bool isListening() const { return m_listening; }

signals:
    /**
     * Note that there is a reason for passing delta time instead of absolute time.
     * Animation timers do not necessarily stop when blocked, and animations
     * shouldn't generally depend on absolute time. In case absolute time is
     * needed, in can be computed as a sum of delta values.
     *
     * @param deltaMs Time that has passed since the last tick in milliseconds.
     */
    void tick(int deltaMs);

private:
    /** Private constructor, so instances are to be created using shared pointer via ::create(). */
    AnimationTimerListener();

    /** Internal function which is to be called by AnimationTimer. */
    void doTick(int deltaMs);

    friend class AnimationTimer;

    AnimationTimer* m_timer = nullptr;
    bool m_listening = false;
};

using AnimationTimerListenerPtr = std::shared_ptr<AnimationTimerListener>;
