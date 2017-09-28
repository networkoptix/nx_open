#pragma once

#include <limits>

#include <QtCore/QElapsedTimer>
#include <QtCore/QLinkedList>
#include <QtCore/QPair>
#include <QtCore/QtMath>
#include <QDebug>

namespace nx {
namespace client {
namespace core {
namespace animation {

// TODO: #dklychkov #vkutin Add an option to use external time source instead of m_timer.

template<class T>
class KineticHelper
{
public:
    enum State
    {
        Stopped,
        Measuring,
        Moving
    };

    static const int maxMoves = 8; // TODO: Refactor.

public:
    KineticHelper() = default;

    qreal minSpeed() const
    {
        return m_minSpeed;
    }

    void setMinSpeed(qreal speed)
    {
        m_minSpeed = speed;
    }

    qreal maxSpeed() const
    {
        return m_maxSpeed;
    }

    void setMaxSpeed(qreal speed)
    {
        m_maxSpeed = speed;
    }

    qreal epsilonSpeed() const
    {
        return m_epsilonSpeed;
    }

    void setEpsilonSpeed(qreal speed)
    {
        m_epsilonSpeed = speed;
    }

    void setMinimum(T minValue)
    {
        m_minValue = minValue;
    }

    void setMaximum(T maxValue)
    {
        m_maxValue = maxValue;
    }

    qreal deceleration() const
    {
        return m_deceleration;
    }

    void setDeceleration(qreal deceleration)
    {
        m_deceleration = deceleration;
    }

    void start(T startValue)
    {
        stop();
        m_moves.append(Move(startValue, 0));
        m_value = startValue;
        m_timer.start();
        m_state = Measuring;
    }

    void move(T targetValue)
    {
        if (m_state != Measuring)
            return;

        m_moves.append(Move(targetValue, m_timer.elapsed()));
        m_value = targetValue;
        if (m_moves.size() > maxMoves)
            m_moves.removeFirst();
    }

    void finish(T targetValue, int fakeTimeShift = 0)
    {
        if (m_state != Measuring)
            return;

        m_moves.append(Move(targetValue, m_timer.elapsed() + fakeTimeShift));

        const T sum = m_moves.last().first - m_moves.first().first;
        const qreal time = m_moves.last().second - m_moves.first().second;

        if (qFuzzyIsNull(time))
        {
            m_state = Stopped;
        }
        else
        {
            m_startSpeed = qBound(m_minSpeed, sum / time, m_maxSpeed);
            m_startValue = m_value = targetValue;

            m_state = qAbs(m_startSpeed) < m_epsilonSpeed ? Stopped : Moving;
            m_timer.restart();
        }
    }

    void flick(T targetValue, qreal speed)
    {
        stop();

        m_startSpeed = qBound(m_minSpeed, speed, m_maxSpeed);
        m_startValue = m_value = targetValue;

        m_state = qFuzzyIsNull(m_startSpeed) ? Stopped : Moving;
        m_timer.restart();
    }

    void stop()
    {
        m_moves.clear();
        m_state = Stopped;
    }

    T value() const
    {
        return m_value;
    }

    T finalPos() const
    {
        qreal dir = m_startSpeed >=0 ? -1.0 : 1.0;
        qreal dt = -dir * m_startSpeed / m_deceleration;
        qreal speedDelta = m_deceleration * dt;
        return qBound<qreal>(m_minValue, m_startValue + (m_startSpeed + dir * speedDelta / 2) * dt, m_maxValue);
    }

    void update()
    {
        if (m_state != Moving)
            return;

        qreal dt = m_timer.nsecsElapsed() / 1000000.0;
        qreal dir = m_startSpeed >=0 ? -1.0 : 1.0;
        qreal speedDelta = m_deceleration * dt;

        if (qAbs(m_startSpeed) <= speedDelta)
        {
            speedDelta = qAbs(m_startSpeed);
            dt = speedDelta / m_deceleration;
            m_state = Stopped;
        }
        m_value = qBound<qreal>(m_minValue, m_startValue + (m_startSpeed + dir * speedDelta / 2) * dt, m_maxValue);

        if ((dir < 0.0 && m_value == m_minValue) ||
            (dir > 0.0 && m_value == m_maxValue))
        {
            m_state = Stopped;
        }
    }

    bool isStopped() const
    {
        return m_state == Stopped;
    }

    bool isMeasuring() const
    {
        return m_state == Measuring;
    }

    bool isMoving() const
    {
        return m_state == Moving;
    }

private:
    typedef QPair<T, int> Move;

    QElapsedTimer m_timer;
    QLinkedList<Move> m_moves;
    T m_startValue = T();
    T m_value = T();
    qreal m_startSpeed = 0.0;
    qreal m_deceleration = 0.01;
    State m_state = Stopped;
    qreal m_minSpeed = std::numeric_limits<qreal>::lowest();
    qreal m_maxSpeed = std::numeric_limits<qreal>::max();
    qreal m_epsilonSpeed = 0.3;

    T m_minValue = std::numeric_limits<T>::lowest();
    T m_maxValue = std::numeric_limits<T>::max();
};

} // namespace animation
} // namespace core
} // namespace client
} // namespace nx
