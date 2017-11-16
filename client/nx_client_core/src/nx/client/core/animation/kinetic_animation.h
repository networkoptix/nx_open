#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointF>

#include "kinetic_helper.h"

namespace nx {
namespace client {
namespace core {
namespace animation {

class KineticAnimation: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged)
    Q_PROPERTY(qreal deceleration READ deceleration WRITE setDeceleration)
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

    qreal maximumSpeed() const;
    void setMaximumSpeed(qreal value);

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
} // namespace core
} // namespace client
} // namespace nx
