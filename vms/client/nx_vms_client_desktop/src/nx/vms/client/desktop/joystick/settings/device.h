// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QString>

class QTimer;

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;
struct AxisDescriptor;
class Manager;

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
        Manager* manager = nullptr);

    /** Destructor. */
    virtual ~Device();

    QString id() const;

    /** Device model name. */
    QString modelName() const;

    /** Device path. */
    QString path() const;

    /** Device is initialized. */
    virtual bool isValid() const = 0;

    /** Is Z-axis was found on the device. */
    int zAxisIsInitialized() const;

    /** The number of buttons which has been returned by device. */
    qsizetype initializedButtonsNumber() const;

    void updateStickAxisLimits(const JoystickDescriptor& modelInfo);

    StickPosition currentStickPosition() const;

    /** Set sticks to zero-point and buttons to released state. **/
    void resetState();

signals:
    /** i-th button on the device has been pressed. */
    void buttonPressed(int id);

    /** i-th button on the device has been released. */
    void buttonReleased(int id);

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

    void axisIsInitializedChanged(AxisIndexes index);
    void initializedButtonsNumberChanged();

protected:
    virtual State getNewState() = 0;
    virtual AxisLimits parseAxisLimits(
        const AxisDescriptor& descriptor,
        const AxisLimits& oldLimits) const = 0;
    double mapAxisState(int rawValue, const AxisLimits& limits);
    void processNewState(const State& newState);
    void pollData();

    bool axisIsInitialized(AxisIndexes index) const;
    void setAxisInitialized(AxisIndexes index, bool isInitialized);
    void setInitializedButtonsNumber(int number);

protected:
    Manager* m_manager;
    QString m_id;
    QString m_modelName;
    QString m_path;

    std::array<AxisLimits, axisIndexCount> m_axisLimits;

    StickPosition m_stickPosition;
    ButtonStates m_buttonStates;

private:
    bool m_xAxisInitialized = false;
    bool m_yAxisInitialized = false;
    bool m_zAxisInitialized = false;

    int m_initializedButtonsNumber = 0;
};

using DevicePtr = QSharedPointer<Device>;

} // namespace nx::vms::client::desktop::joystick
