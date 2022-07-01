// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;
struct AxisDescriptor;

class Device: public QObject
{
    Q_OBJECT

public:
    enum AxisIndexes
    {
        xIndex,
        yIndex,
        zIndex,
        axisIndexCount
    };

    using StickPosition = std::array<double, axisIndexCount>;
    using ButtonStates = std::vector<bool>;
    struct State
    {
        StickPosition stickPosition;
        ButtonStates buttonStates;
    };

protected:
    struct AxisLimits
    {
        int min = 0;
        int max = 0;
        int mid = 0;
        int bounce = 0;
        double sensitivity = 0;
    };

    // QString.toInt() could autodetect integer base by their format (0x... for hex numbers, etc.).
    static constexpr int kAutoDetectBase = 0;

    // Minimum significant zero point deviation in percent.
    static constexpr int kMinBounceInPercentages = 2;

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

    StickPosition currentStickPosition() const;

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
    void stateChanged(const StickPosition& stick, const ButtonStates& buttons);

    /**
     * Device request failed, probably it was disconnected.
     */
    void failed();

protected:
    virtual State getNewState() = 0;
    virtual AxisLimits parseAxisLimits(
        const AxisDescriptor& descriptor,
        const AxisLimits& oldLimits) const = 0;
    double mapAxisState(int rawValue, const AxisLimits& limits);
    void pollData();

protected:
    QString m_id;
    QString m_modelName;
    QString m_path;

    std::array<AxisLimits, axisIndexCount> m_axisLimits;

    StickPosition m_stickPosition;
    ButtonStates m_buttonStates;
};

using DevicePtr = QSharedPointer<Device>;

} // namespace nx::vms::client::desktop::joystick
