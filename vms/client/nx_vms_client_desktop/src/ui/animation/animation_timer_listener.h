// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class AnimationTimer;

/**
 * Interface for detached animation processing.
 *
 * It is to be used in conjunction with <tt>AnimationTimer</tt>.
 */
class AnimationTimerListener
{
public:
    AnimationTimerListener();

    virtual ~AnimationTimerListener();

    void setTimer(AnimationTimer* timer);

    AnimationTimer* timer() const { return m_timer; }

protected:
    /**
     * Note that there is a reason for passing delta time instead of absolute time.
     * Animation timers do not necessarily stop when blocked, and animations
     * shouldn't generally depend on absolute time. In case absolute time is
     * needed, in can be computed as a sum of delta values.
     *
     * \param deltaMSecs                Time that has passed since the last tick,
     *                                  in milliseconds.
     */
    virtual void tick(int deltaMSecs) = 0;

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
     * \returns Whether this listener is open for receiving tick notifications.
     */
    bool isListening() const { return m_listening; }

private:
    friend class AnimationTimer;

    AnimationTimer* m_timer = nullptr;
    bool m_listening = false;
};
