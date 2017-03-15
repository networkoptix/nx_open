#pragma once

#if defined(Q_OS_WIN)

#include "abstract_joystick_driver.h"

#include <WinUser.h>

#include <boost/optional/optional.hpp>

#include <set>

#include <nx/utils/singleton.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace driver {

class MmWinDriver:
    public AbstractJoystickDriver,
    public Singleton<MmWinDriver>
{

public:
    MmWinDriver(HWND windowId);
    virtual ~MmWinDriver() override;

    virtual std::vector<JoystickPtr> enumerateJoysticks() override;
    virtual bool captureJoystick(JoystickPtr& joystickToCapture) override;
    virtual bool releaseJoystick(JoystickPtr& joystickToRelease) override;
    virtual bool setControlState(
        const QString& joystickId,
        const QString& controlId,
        const State& state) override;

    void notifyJoystickStateChanged(const JOYINFOEX& info, uint joystickIndex);
    void notifyHardwareConfigurationChanged();

private:
    JoystickPtr getJoystickByIndexUnsafe(uint joystickIndex) const;
    boost::optional<uint> getJoystickIndex(const JoystickPtr& joy) const;

    JoystickPtr createJoystick(
        JOYINFO joystickInfo,
        JOYCAPS joystickCapabitlities,
        int joystickIndex);

    void notifyJoystickButtonsStateChanged(JoystickPtr& joy, DWORD buttonStates);
    void notifyJoystickSticksStateChanged(JoystickPtr& joy, const JOYINFOEX& info);

    QString makeId(const QString& objectType, uint objectIndex);

private:
    HWND m_windowId;
    std::set<uint> m_capturedJoysticks;
    std::vector<JoystickPtr> m_joysticks;
    mutable QnMutex m_mutex;
};

} // namespace driver
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx

#endif // defined(Q_OS_WIN)