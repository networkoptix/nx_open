#if defined(Q_OS_WIN)

#include "mm_joystick_driver_win.h"
#include "generic_joystick.h"
#include <plugins/io_device/joystick/controls/joystick_button_control.h>
#include <plugins/io_device/joystick/controls/joystick_stick_control.h>

namespace {

const uint kMaxJoystickIndex = 1;
const uint kMaxButtonNumber = 32;
const uint kMaxStickNumber = 2;
const QString kJoystickObjectType = lit("joystick");
const QString kButtonObjectType = lit("button");
const QString kStickObjectType = lit("stick");

} // namespace 

namespace nx {
namespace joystick {
namespace driver {

MmWinDriver::MmWinDriver(HWND windowId):
    m_windowId(windowId)
{

}

MmWinDriver::~MmWinDriver()
{
    for (auto capturedJoystickIndex: m_capturedJoysticks)
        joyReleaseCapture(capturedJoystickIndex);
}

std::vector<JoystickPtr> MmWinDriver::enumerateJoysticks()
{
    auto maxJoysticks = joyGetNumDevs();
    auto result = JOYERR_NOERROR;

    if (maxJoysticks <= 0)
        return m_joysticks;

    for (auto i = 0; i < maxJoysticks; ++i)
    {
        JOYINFO joystickInfo;
        if (joyGetPos(i, &joystickInfo) != JOYERR_NOERROR)
            continue;

        JOYCAPS joystickCapabilities;
        if (joyGetDevCaps(0, &joystickCapabilities, sizeof(joystickCapabilities)) != JOYERR_NOERROR)
            continue;

        auto joy = createJoystick(joystickInfo, joystickCapabilities, i);
        if (!joy)
            continue;

        m_joysticks.push_back(joy);
    }

    return m_joysticks;
}

bool MmWinDriver::captureJoystick(JoystickPtr& joystickToCapture)
{
    auto joystickIndex = getJoystickIndex(joystickToCapture);
    if (!joystickIndex)
        return false;

    const uint kDefaultPeriodMs = 100;
    auto result = joySetCapture(m_windowId, joystickIndex.get(), kDefaultPeriodMs, false);

    if (result != JOYERR_NOERROR)
        return false;

    m_capturedJoysticks.insert(joystickIndex.get());

    return true;
}

bool MmWinDriver::releaseJoystick(JoystickPtr& joystickToRelease)
{
    auto joystickIndex = getJoystickIndex(joystickToRelease);
    if (!joystickIndex)
        return false;

    auto result = joyReleaseCapture(joystickIndex.get());

    if (result != JOYERR_NOERROR)
        return false;

    m_capturedJoysticks.erase(joystickIndex.get());

    return true;
}

bool MmWinDriver::setControlState(const QString& controlId, const nx::joystick::State& state)
{
    return true;
}

void MmWinDriver::updateJoystickState(const JOYINFOEX& info, uint joystickIndex)
{
    auto joy = getJoystickByIndex(joystickIndex);

    if (!joy)
        return;
    
    updateJoystickButtons(joy, info.dwButtons);
    updateJoystickSticks(joy, info);
}

nx::joystick::JoystickPtr MmWinDriver::getJoystickByIndex(uint joystickIndex) const
{
    if (joystickIndex > kMaxJoystickIndex)
        return nullptr;

    if (joystickIndex >= m_joysticks.size())
        return nullptr;

    return m_joysticks[joystickIndex];
}

boost::optional<uint> MmWinDriver::getJoystickIndex(const JoystickPtr& joy) const
{
    auto id = joy->getId();
    auto split = id.split(L':');

    if (split.size() != 2)
        return boost::none;

    bool success = false;
    auto index = split[1].toInt(&success);

    if (!success)
        return boost::none;

    return index;
}

JoystickPtr MmWinDriver::createJoystick(
    JOYINFO joystickInfo,
    JOYCAPS joystickCapabitlities,
    int joystickIndex)
{
    auto joy = std::make_shared<GenericJoystick>();
    joy->setVendor(lit("Generic vendor"));
    joy->setModel(lit("Generic model"));
    joy->setId(makeId(kJoystickObjectType, joystickIndex));

    std::vector<controls::ButtonPtr> buttons;
    for (auto i = 0; i < joystickCapabitlities.wNumButtons; ++i)
    {
        auto button = std::make_shared<controls::Button>();
        button->setId(makeId(kButtonObjectType, i));
        button->setDescription(lit("Generic button"));
        buttons.push_back(button);
    }

    joy->setButtons(buttons);

    std::vector<controls::StickPtr> sticks;
    auto stick = std::make_shared<controls::Stick>();
    stick->setId(makeId(kStickObjectType, 0));
    stick->setDescription(lit("Generic stick"));
    stick->setStickXLimits(joystickCapabitlities.wXmin, joystickCapabitlities.wXmax);
    stick->setStickYLimits(joystickCapabitlities.wYmin, joystickCapabitlities.wYmax);
    stick->setStickZLimits(joystickCapabitlities.wZmin, joystickCapabitlities.wZmax);
    stick->setStickRxLimits(joystickCapabitlities.wRmin, joystickCapabitlities.wRmax);
    stick->setStickRyLimits(joystickCapabitlities.wUmin, joystickCapabitlities.wUmax);
    stick->setStickRzLimits(joystickCapabitlities.wVmin, joystickCapabitlities.wVmax);

    nx::joystick::controls::LimitsVector outputLimits;
    for (auto i = 0; i < 6; ++i)
        outputLimits.emplace_back(-100, 100);

    stick->setOutputLimits(outputLimits);
    sticks.push_back(stick);

    joy->setSticks(sticks);
    joy->setDriver(this);

    return joy; 
}

void MmWinDriver::updateJoystickButtons(JoystickPtr& joy, DWORD buttonStates)
{
    DWORD currentButtonMask = 1;
    int currentButtonIndex = 0;

    if (!joy)
        return;

    while (currentButtonIndex < kMaxButtonNumber)
    {
        auto controlId = makeId(kButtonObjectType, currentButtonIndex);
        if (!joy->getControlById(controlId))
            return;

        nx::joystick::StateAtom pressedState = (currentButtonMask & buttonStates) ? 1 : 0;
        nx::joystick::State state;
        state.push_back(pressedState);
        joy->updateControlStateWithRawValue(controlId, state);
        ++currentButtonIndex;
        currentButtonMask *= 2;
    }
}

void MmWinDriver::updateJoystickSticks(JoystickPtr& joy, const JOYINFOEX& info)
{
    auto controlId = makeId(kStickObjectType, 0);
    if (!joy->getControlById(controlId))
        return;

    nx::joystick::State state;
    state.push_back(info.dwXpos);
    state.push_back(info.dwYpos);
    state.push_back(info.dwZpos);
    state.push_back(info.dwRpos);
    state.push_back(info.dwUpos);
    state.push_back(info.dwVpos);

    joy->updateControlStateWithRawValue(controlId, state);
}

QString MmWinDriver::makeId(const QString& objectType, uint objectIndex)
{
    return lit("%1:%2")
        .arg(objectType)
        .arg(objectIndex);
}

} // namespace driver
} // namespace joystick
} // namespace nx

#endif