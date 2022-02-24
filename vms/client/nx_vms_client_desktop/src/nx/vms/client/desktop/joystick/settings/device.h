// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;
struct AxisDescriptor;

class Device: public QObject
{
    Q_OBJECT

protected:
    struct AxisLimits
    {
        int min = 0;
        int max = 0;
        int mid = 0;
        int bounce = 0;
        double sensitivity = 0;
    };

public:
    /**
     * Constructor for supported joystick models.
     */
    Device(
        const JoystickDescriptor& modelInfo,
        const QString& path,
        QTimer* pollTimer,
        QObject* parent = 0);

    /** Destructor. */
    virtual ~Device();

    QString id() const;

    /** Device model name. */
    QString modelName() const;

    /** Device path. */
    QString path() const;

    /** Device is initialized. */
    virtual bool isValid() const = 0;

    void updateStickAxisLimits(const JoystickDescriptor& modelInfo);

signals:
    /** i-th button on the device has been pressed. */
    void buttonPressed(int id);

    /** i-th button on the device has been released. */
    void buttonReleased(int id);

    /**
     * Stick has been moved.
     * x, y and z are values in range [-1, 1], where 0 corresponds to neutral stick position.
     */
    void stickMoved(double x, double y, double z);

    /**
     * State of any button has changed or the stick has been moved.
     * This signal is necessary for command processing since we need to check modifier state
     * changes before processing any other button state change.
     */
    void stateChanged(const std::vector<double>& stick, const std::vector<bool>& buttons);

protected:
    virtual QPair<std::vector<double>, std::vector<bool>> getNewState() = 0;
    virtual AxisLimits parseAxisLimits(const AxisDescriptor& descriptor) = 0;
    double mapAxisState(int rawValue, const AxisLimits& limits);
    void pollData();

protected:
    Mutex m_mutex;

    QString m_id;
    QString m_modelName;
    QString m_path;

    std::vector<AxisLimits> m_axisLimits;

    std::vector<double> m_stickPosition;
    std::vector<bool> m_buttonStates;
};

using DevicePtr = QSharedPointer<Device>;

} // namespace nx::vms::client::desktop::joystick
