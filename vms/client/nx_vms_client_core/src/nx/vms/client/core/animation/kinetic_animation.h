// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointF>

#include "kinetic_helper.h"

namespace nx::vms::client::core {
namespace animation {

class NX_VMS_CLIENT_CORE_API KineticAnimation: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged)
    Q_PROPERTY(QPointF startPosition READ startPosition NOTIFY measurementStarted)
    Q_PROPERTY(qreal deceleration READ deceleration WRITE setDeceleration)
    Q_PROPERTY(qreal epsilonSpeed READ epsilonSpeed WRITE setEpsilonSpeed)
    Q_PROPERTY(qreal maximumSpeed READ maximumSpeed WRITE setMaximumSpeed)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    enum State
    {
        Stopped,
        Measuring,
        Running
    };
    Q_ENUM(State)

public:
    KineticAnimation();
    virtual ~KineticAnimation() override;

    Q_INVOKABLE void startMeasurement(const QPointF& position);
    Q_INVOKABLE void continueMeasurement(const QPointF& position);
    Q_INVOKABLE void finishMeasurement(const QPointF& position);

    Q_INVOKABLE void stop();

    // Current position.
    QPointF position() const;

    // Starting position (that was passed to startMeasurement).
    QPointF startPosition() const;

    State state() const;

    qreal deceleration() const;
    void setDeceleration(qreal value);

    qreal epsilonSpeed() const;
    void setEpsilonSpeed(qreal value);

    qreal maximumSpeed() const;
    void setMaximumSpeed(qreal value);

    static void registerQmlType();

signals:
    // Emitted every time when current position changes,
    // during both measurement and inertial run.
    void positionChanged(const QPointF& position);

    // Emitted when measurement is started.
    void measurementStarted();

    // Emitted when measurement is finished and inertial run is started.
    void inertiaStarted();

    // Emitted when inertial run is finished.
    void stopped();

    // Emitted when state is changed.
    void stateChanged();

private:
    KineticHelper<qreal> m_xHelper;
    KineticHelper<qreal> m_yHelper;

    class Animation;
    Animation* const m_animation;

    QPointF m_startPosition{};
    State m_state = State::Stopped;
};

} // namespace animation
} // namespace nx::vms::client::core
