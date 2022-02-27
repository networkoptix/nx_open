// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtGui/QVector2D>

namespace nx::vms::client::core {

class VelocityMeter: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector2D velocity READ velocity NOTIFY velocityChanged)

public:
    VelocityMeter(QObject* parent = nullptr);

    Q_INVOKABLE void start(const QPointF& point, const qint64 timestampMs = -1);
    Q_INVOKABLE void addPoint(const QPointF& point, const qint64 timestampMs = -1);

    /**
     * @return Velocity in "units" per second.
     */
    QVector2D velocity() const { return m_velocity; }

    static void registerQmlType();

signals:
    void velocityChanged();

private:
    QElapsedTimer m_timer;
    qint64 m_prevTimestampMs = -1;
    QPointF m_prevPoint;
    QVector2D m_velocity;
};

} // namespace nx::vms::client::core
