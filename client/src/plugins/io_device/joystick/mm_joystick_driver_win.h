#pragma once

#if defined(Q_OS_WIN)

#include "abstract_joystick_driver.h"

#include <WinUser.h>

#include <boost/optional/optional.hpp>

#include <set>

#include <utils/common/singleton.h>

namespace nx {
namespace joystick {
namespace  driver {

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
    virtual bool setControlState(const QString& controlId, const nx::joystick::State& state) override;

    void updateJoystickState(const JOYINFOEX& info, uint joystickIndex);

private:
    JoystickPtr getJoystickByIndex(uint joystickIndex) const;
    boost::optional<uint> getJoystickIndex(const JoystickPtr& joy) const;

    JoystickPtr createJoystick(
        JOYINFO joystickInfo,
        JOYCAPS joystickCapabitlities,
        int joystickIndex);

    void updateJoystickButtons(JoystickPtr& joy, DWORD buttonStates);
    void updateJoystickSticks(JoystickPtr& joy, const JOYINFOEX& info);

    QString makeId(const QString& objectType, uint objectIndex);

private:
    HWND m_windowId;
    std::set<uint> m_capturedJoysticks;
    std::vector<JoystickPtr> m_joysticks;
};

} // namespace driver
} // namespace joystick
} // namespace nx

#endif // defined(Q_OS_WIN)