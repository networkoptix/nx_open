// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "velocity_meter.h"

#include <QtCore/QTime>
#include <QtQml/QtQml>

namespace nx::vms::client::core {

VelocityMeter::VelocityMeter(QObject* parent):
    QObject(parent)
{
}

void VelocityMeter::start(const QPointF& point, const qint64 timestampMs)
{
    if (timestampMs >= 0)
    {
        m_prevTimestampMs = timestampMs;
    }
    else
    {
        m_prevTimestampMs = 0;
        m_timer.start();
    }

    m_prevPoint = point;
    m_velocity = QVector2D();
    emit velocityChanged();
}

void VelocityMeter::addPoint(const QPointF& point, const qint64 timestampMs)
{
    const auto ts = m_timer.isValid() ? m_timer.elapsed() : timestampMs;
    const auto elapsed = ts - m_prevTimestampMs;

    QVector2D velocity;

    if (elapsed > 0)
    {
        velocity = QVector2D((point - m_prevPoint) / elapsed) * 1000;

        // A primitive Kalman filter.
        constexpr qreal kKalmanGain = 0.7;
        m_velocity = velocity * kKalmanGain + m_velocity * (1 - kKalmanGain);
        emit velocityChanged();
    }

    m_prevTimestampMs = ts;
    m_prevPoint = point;
}

void VelocityMeter::registerQmlType()
{
    qmlRegisterType<VelocityMeter>("nx.vms.client.core", 1, 0, "VelocityMeter");
}

} // namespace nx::vms::client::core
