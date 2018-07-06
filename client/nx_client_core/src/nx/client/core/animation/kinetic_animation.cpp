#include "kinetic_animation.h"

#include <QtCore/QAbstractAnimation>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace nx {
namespace client {
namespace core {
namespace animation {

class KineticAnimation::Animation: public QAbstractAnimation
{
public:
    Animation(KineticAnimation* parent):
        QAbstractAnimation(parent),
        m_main(parent)
    {
    }

    virtual int duration() const override
    {
        return -1; //< Unlimited duration.
    }

    virtual void updateCurrentTime(int /*currentTime*/) override
    {
        const auto previous = m_main->position();
        m_main->m_xHelper.update();
        m_main->m_yHelper.update();

        const auto current = m_main->position();
        if (!qFuzzyEquals(previous, current))
            emit m_main->positionChanged(current);

        if (!m_main->m_xHelper.isMoving() && !m_main->m_yHelper.isMoving())
            stop();
    }

    virtual void updateState(QAbstractAnimation::State newState,
        QAbstractAnimation::State oldState) override
    {
        if (oldState == newState)
            return;

        switch (newState)
        {
            case QAbstractAnimation::Stopped:
                emit m_main->stopped();
                break;

            case QAbstractAnimation::Running:
                emit m_main->inertiaStarted();
                break;

            default:
                NX_ASSERT(false);
                break;
        }
    }

private:
    KineticAnimation* const m_main;
};

KineticAnimation::KineticAnimation():
    m_animation(new Animation(this))
{
}

KineticAnimation::~KineticAnimation()
{
}

QPointF KineticAnimation::position() const
{
    return QPointF(
        m_xHelper.value(),
        m_yHelper.value());
}

qreal KineticAnimation::deceleration() const
{
    return m_xHelper.deceleration();
}

void KineticAnimation::setDeceleration(qreal value)
{
    m_xHelper.setDeceleration(value);
    m_yHelper.setDeceleration(value);
}

qreal KineticAnimation::maximumSpeed() const
{
    return m_xHelper.maxSpeed();
}

void KineticAnimation::setMaximumSpeed(qreal value)
{
    value = qAbs(value);
    m_xHelper.setMaxSpeed(value);
    m_yHelper.setMaxSpeed(value);
    m_xHelper.setMinSpeed(-value);
    m_yHelper.setMinSpeed(-value);
}

void KineticAnimation::startMeasurement(const QPointF& position)
{
    m_animation->stop();
    m_xHelper.start(position.x());
    m_yHelper.start(position.y());
    emit measurementStarted();
    emit positionChanged(position);
}

void KineticAnimation::continueMeasurement(const QPointF& position)
{
    m_xHelper.move(position.x());
    m_yHelper.move(position.y());
    emit positionChanged(position);
}

void KineticAnimation::finishMeasurement(const QPointF& position)
{
    m_xHelper.finish(position.x());
    m_yHelper.finish(position.y());
    m_animation->start();
}

void KineticAnimation::stop()
{
    if (m_animation->state() != QAbstractAnimation::Stopped
        || !m_xHelper.isStopped() || !m_yHelper.isStopped())
    {
        m_animation->stop();
        m_xHelper.stop();
        m_yHelper.stop();
        emit stopped();
    }
}

} // namespace animation
} // namespace core
} // namespace client
} // namespace nx
