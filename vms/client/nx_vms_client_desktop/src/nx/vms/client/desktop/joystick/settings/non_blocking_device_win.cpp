// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_blocking_device_win.h"

#include <QtCore/QPair>

#include <nx/utils/log/log.h>

#include "manager.h"
#include "osal/osal_driver.h"

namespace nx::vms::client::desktop::joystick {

class NonBlockingDeviceWinStateListener: public OsalDeviceListener
{
public:
    NonBlockingDeviceWinStateListener(NonBlockingDeviceWin* device)
        :
        OsalDeviceListener(device),
        m_device(device)
    {
    }

    virtual void onStateChanged(const OsalDevice::State& newState) override
    {
        m_device->onStateChanged(newState);
    }

private:
    NonBlockingDeviceWin* m_device;
};

struct NonBlockingDeviceWin::Private
{
    NonBlockingDeviceWin* const q;

    OsalDeviceListener* stateListener;

    int bufferSize = 0;
    QScopedArrayPointer<unsigned char> buffer;

    std::vector<ParsedFieldLocation> axisLocations;
    std::vector<ParsedFieldLocation> buttonLocations;
};

NonBlockingDeviceWin::NonBlockingDeviceWin(
    const JoystickDescriptor& modelInfo,
    const QString& path,
    Manager* manager)
    :
    base_type(modelInfo, path, nullptr, manager),
    d(new Private{.q = this, .stateListener = new NonBlockingDeviceWinStateListener(this)})
{
    OsalDriver::getDriver()->setupDeviceListener(path, d->stateListener);

    for (int axisIndex = 0; axisIndex < Device::axisIndexCount; ++axisIndex)
        setAxisInitialized((Device::AxisIndexes)axisIndex, true);

    setInitializedButtonsNumber(modelInfo.buttons.size());

    // Initialize stick axes (x, y, z).
    d->axisLocations.push_back(parseLocation(modelInfo.xAxis.bits));
    d->axisLocations.push_back(parseLocation(modelInfo.yAxis.bits));
    d->axisLocations.push_back(parseLocation(modelInfo.zAxis.bits));
    for (const auto& button: modelInfo.buttons)
        d->buttonLocations.push_back(parseLocation(button.bits));

    updateStickAxisLimits(modelInfo);
}

NonBlockingDeviceWin::~NonBlockingDeviceWin()
{
    OsalDriver::getDriver()->removeDeviceListener(d->stateListener);
}

bool NonBlockingDeviceWin::isValid() const
{
    // We don't need this method with non-polling device.
    return true;
}

void NonBlockingDeviceWin::onStateChanged(const OsalDevice::State& state)
{
    if (!m_manager->isActive())
    {
        NX_DEBUG(this, "Device manager is not active, ignoring the state change.");
        return;
    }

    std::vector<bool> buttonStates;
    for (int i = 0; i < state.buttons.count(); i++)
        buttonStates.push_back(state.buttons[i]);

    formAxisLimits(state.minX, state.maxX, &m_axisLimits[xIndex]);
    formAxisLimits(state.minY, state.maxY, &m_axisLimits[yIndex]);
    formAxisLimits(state.minZ, state.maxZ, &m_axisLimits[zIndex]);

    StickPosition newStickPosition{
        mapAxisState(state.x, m_axisLimits[xIndex]),
        mapAxisState(state.y, m_axisLimits[yIndex]),
        mapAxisState(state.z, m_axisLimits[zIndex])
    };

    const State newState = { .stickPosition = newStickPosition, .buttonStates = buttonStates };

    processNewState(newState);
}

Device::State NonBlockingDeviceWin::getNewState()
{
    NX_ASSERT(false, "This method should no be used in case of non-polling device.");
    return {};
}

Device::AxisLimits NonBlockingDeviceWin::parseAxisLimits(
    const AxisDescriptor& descriptor,
    const AxisLimits& /*oldLimits*/) const
{
    Device::AxisLimits result;
    result.min = descriptor.min.toInt(/*ok*/ nullptr, kAutoDetectBase);
    result.max = descriptor.max.toInt(/*ok*/ nullptr, kAutoDetectBase);
    result.mid = descriptor.mid.toInt(/*ok*/ nullptr, kAutoDetectBase);

    result.bounce = descriptor.bounce.toInt(/*ok*/ nullptr, kAutoDetectBase);

    const int minBounce = (result.max - result.min) / 100.0 * kMinBounceInPercentages;
    if (minBounce > result.bounce)
        result.bounce = minBounce;

    if (result.mid == 0)
        result.mid = (result.min + result.max) / 2;
    result.sensitivity = descriptor.sensitivity.toDouble(/*ok*/ nullptr);
    return result;
}

NonBlockingDeviceWin::ParsedFieldLocation NonBlockingDeviceWin::parseLocation(
    const FieldLocation& location)
{
    NonBlockingDeviceWin::ParsedFieldLocation result;

    for (const auto& interval: location.split(',', Qt::SkipEmptyParts))
    {
        auto borders = interval.split('-');

        // Check for intervals with omitted 'last' part.
        if (borders.size() == 1)
            borders << borders.first();

        if (borders.size() != 2)
            return {};

        bool ok1 = false;
        bool ok2 = false;
        int first = borders[0].toInt(&ok1, kAutoDetectBase);
        int last = borders[1].toInt(&ok2, kAutoDetectBase);

        if (!ok1 || !ok2)
            return {};

        result.intervals.push_back({first, last});
        // Calc and store the field size.
        // It never changes after initialization, so there's no need for a separate function.
        result.size += last - first + 1;
    }

    return result;
}

void NonBlockingDeviceWin::formAxisLimits(int min, int max, Device::AxisLimits* limits)
{
    limits->min = min;
    limits->max = max;
    limits->mid = (max - min) / 2;

    const int minBounce = (max - min) / 100.0 * kMinBounceInPercentages;
    if (minBounce > limits->bounce)
        limits->bounce = minBounce;
}

} // namespace nx::vms::client::desktop::joystick
