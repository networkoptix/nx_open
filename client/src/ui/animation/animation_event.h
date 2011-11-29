#ifndef QN_ANIMATION_EVENT_H
#define QN_ANIMATION_EVENT_H

#include <QEvent>

class AnimationEvent: public QEvent {
public:
    AnimationEvent(qint64 time, qint64 deltaTime):
        m_time(time),
        m_deltaTime(deltaTime)
    {}

    /** Event type for animation events. */
    static const QEvent::Type Animation = static_cast<QEvent::Type>(QEvent::User + 0x6427);

    qint64 time() const {
        return m_time;
    }

    qint64 deltaTime() const {
        return m_deltaTime;
    }

private:
    qint64 m_time;
    qint64 m_deltaTime;

};


#endif // QN_ANIMATION_EVENT_H
