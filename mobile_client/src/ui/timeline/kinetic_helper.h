#ifndef QNKINETICHELPER_H
#define QNKINETICHELPER_H

#include <limits>

#include <QtCore/QElapsedTimer>
#include <QtCore/QLinkedList>
#include <QtCore/QPair>
#include <QtCore/QtMath>
#include <QDebug>

static const int maxMoves = 8;

template<class T>
class QnKineticHelper {
public:
    enum State {
        Stopped,
        Measuring,
        Moving
    };

    typedef QPair<T, int> Move;

    QElapsedTimer m_timer;
    QLinkedList<Move> m_moves;
    T m_startValue;
    T m_value;
    qreal m_startSpeed;
    qreal m_deceleration;
    State m_state;
    qreal m_minSpeed;
    qreal m_maxSpeed;

    T m_minValue;
    T m_maxValue;

public:
    QnKineticHelper() :
        m_deceleration(0.01),
        m_state(Stopped),
        m_maxSpeed(std::numeric_limits<qreal>::max())
    {
        m_minValue = std::numeric_limits<T>::lowest();
        m_maxValue = std::numeric_limits<T>::max();
    }

    qreal minSpeed() const {
        return m_minSpeed;
    }

    void setMinSpeed(qreal speed) {
        m_minSpeed = speed;
    }

    qreal maxSpeed() const {
        return m_maxSpeed;
    }

    void setMaxSpeed(qreal speed) {
        m_maxSpeed = speed;
    }

    void setMinimum(T minValue) {
        m_minValue = minValue;
    }

    void setMaximum(T maxValue) {
        m_maxValue = maxValue;
    }

    qreal deceleration() const {
        return m_deceleration;
    }

    void setDeceleration(qreal deceleration) {
        m_deceleration = deceleration;
    }

    void start(T startValue) {
        stop();
        m_moves.append(Move(startValue, 0));
        m_value = startValue;
        m_timer.start();
        m_state = Measuring;
    }

    void move(T targetValue) {
        if (m_state != Measuring)
            return;

        m_moves.append(Move(targetValue, m_timer.elapsed()));
        m_value = targetValue;
        if (m_moves.size() > maxMoves)
            m_moves.removeFirst();
    }

    void finish(T targetValue, int fakeTimeShift = 0) {
        if (m_state != Measuring)
            return;

        m_moves.append(Move(targetValue, m_timer.elapsed() + fakeTimeShift));

        T sum = m_moves.last().first - m_moves.first().first;
        qreal time = m_moves.last().second - m_moves.first().second;

        m_startSpeed = qBound(m_minSpeed, sum / time, m_maxSpeed);
        m_startValue = m_value = targetValue;

        m_state = qFuzzyIsNull(m_startSpeed) ? Stopped : Moving;
        m_timer.restart();
    }

    void flick(T targetValue, qreal speed) {
        stop();

        m_startSpeed = qBound(m_minSpeed, speed, m_maxSpeed);
        m_startValue = m_value = targetValue;

        m_state = qFuzzyIsNull(m_startSpeed) ? Stopped : Moving;
        m_timer.restart();
    }

    void stop() {
        m_moves.clear();
        m_state = Stopped;
    }

    T value() const {
        return m_value;
    }

    T finalPos() const {
        qreal dir = m_startSpeed >=0 ? -1.0 : 1.0;
        qreal dt = -dir * m_startSpeed / m_deceleration;
        qreal speedDelta = m_deceleration * dt;
        return qBound<qreal>(m_minValue, m_startValue + (m_startSpeed + dir * speedDelta / 2) * dt, m_maxValue);
    }

    void update() {
        if (m_state != Moving)
            return;

        qreal dt = m_timer.nsecsElapsed() / 1000000.0;
        qreal dir = m_startSpeed >=0 ? -1.0 : 1.0;
        qreal speedDelta = m_deceleration * dt;

        if (qAbs(m_startSpeed) <= speedDelta) {
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

    bool isStopped() const {
        return m_state == Stopped;
    }

    bool isMeasuring() const {
        return m_state == Measuring;
    }
};

#endif // QNKINETICHELPER_H
