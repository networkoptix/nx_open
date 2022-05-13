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
    Q_PROPERTY(qreal deceleration READ deceleration WRITE setDeceleration)
    Q_PROPERTY(qreal epsilonSpeed READ epsilonSpeed WRITE setEpsilonSpeed)
    Q_PROPERTY(qreal maximumSpeed READ maximumSpeed WRITE setMaximumSpeed)

public:
    KineticAnimation();
    virtual ~KineticAnimation() override;

    Q_INVOKABLE void startMeasurement(const QPointF& position);
    Q_INVOKABLE void continueMeasurement(const QPointF& position);
    Q_INVOKABLE void finishMeasurement(const QPointF& position);

    Q_INVOKABLE void stop();

    // Current position.
    QPointF position() const;

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

private:
    KineticHelper<qreal> m_xHelper;
    KineticHelper<qreal> m_yHelper;

    class Animation;
    Animation* const m_animation;
};

} // namespace animation
} // namespace nx::vms::client::core
