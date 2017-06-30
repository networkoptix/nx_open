#if defined(Q_OS_WIN)

#include "mm_joystick_driver_win.h"
#include <plugins/io_device/joystick/joystick_manager.h>

#include <plugins/io_device/joystick/generic_joystick.h>
#include <plugins/io_device/joystick/controls/joystick_button_control.h>
#include <plugins/io_device/joystick/controls/joystick_stick_control.h>
#include <nx/utils/timer_manager.h>

namespace {

const uint kMaxJoystickIndex = 1;
const uint kMaxButtonNumber = 32;
const uint kMaxStickNumber = 2;
const QString kJoystickObjectType = lit("joystick");
const QString kButtonObjectType = lit("button");
const QString kStickObjectType = lit("stick");
const int kDefaultStickLogicalRangeMin = -100;
const int kDefaultStickLogicalRangeMax = 100;

} // namespace

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace driver {

MmWinDriver::MmWinDriver(HWND windowId):
    m_windowId(windowId)
{

}

MmWinDriver::~MmWinDriver()
{
    QnMutexLocker lock(&m_mutex);
    for (auto capturedJoystickIndex: m_capturedJoysticks)
        joyReleaseCapture(capturedJoystickIndex);
}

std::vector<JoystickPtr> MmWinDriver::enumerateJoysticks()
{
    QnMutexLocker lock(&m_mutex);

    auto maxJoysticks = joyGetNumDevs();
    auto result = JOYERR_NOERROR;

    if (maxJoysticks <= 0)
        return std::vector<JoystickPtr>();

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
    QnMutexLocker lock(&m_mutex);
    auto joystickIndex = getJoystickIndex(joystickToCapture);
    if (!joystickIndex)
        return false;

    const uint kDefaultPeriodMs = 50;
    if (m_capturedJoysticks.find(joystickIndex.get()) != m_capturedJoysticks.end())
        return true;

    auto result = joySetCapture(m_windowId, joystickIndex.get(), kDefaultPeriodMs, false);

    auto hwnd = m_windowId;
    auto jIndex = joystickIndex.get();

    if (result != JOYERR_NOERROR)
    {
        return false;
    }

    m_capturedJoysticks.insert(joystickIndex.get());

    return true;
}

bool MmWinDriver::releaseJoystick(JoystickPtr& joystickToRelease)
{
    QnMutexLocker lock(&m_mutex);
    auto joystickIndex = getJoystickIndex(joystickToRelease);
    if (!joystickIndex)
        return false;

    auto result = joyReleaseCapture(joystickIndex.get());

    if (result != JOYERR_NOERROR)
        return false;

    m_capturedJoysticks.erase(joystickIndex.get());

    return true;
}

bool MmWinDriver::setControlState(
    const QString& joystickId,
    const QString& controlId,
    const State& state)
{
    // No way to set control state via Windows Joystick API, so just ignore these calls.
    return true;
}

void MmWinDriver::notifyJoystickStateChanged(const JOYINFOEX& info, uint joystickIndex)
{
    QnMutexLocker lock(&m_mutex);
    auto joy = getJoystickByIndexUnsafe(joystickIndex);

    if (!joy)
        return;

    notifyJoystickButtonsStateChanged(joy, info.dwButtons);
    notifyJoystickSticksStateChanged(joy, info);
}

void MmWinDriver::notifyHardwareConfigurationChanged()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_joysticks.clear();
    }

    auto manager = Manager::instance();
    manager->notifyHardwareConfigurationChanged();
}

JoystickPtr MmWinDriver::getJoystickByIndexUnsafe(uint joystickIndex) const
{
    if (joystickIndex > kMaxJoystickIndex)
        return nullptr;

    if (joystickIndex >= m_joysticks.size())
        return nullptr;

    return m_joysticks[joystickIndex];
}

boost::optional<uint> MmWinDriver::getJoystickIndex(const JoystickPtr& joy) const
{
    auto id = joy->id();
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
    stick->setStickXRange(joystickCapabitlities.wXmin, joystickCapabitlities.wXmax);
    stick->setStickYRange(joystickCapabitlities.wYmin, joystickCapabitlities.wYmax);
    stick->setStickZRange(joystickCapabitlities.wZmin, joystickCapabitlities.wZmax);
    stick->setStickRxRange(joystickCapabitlities.wRmin, joystickCapabitlities.wRmax);
    stick->setStickRyRange(joystickCapabitlities.wUmin, joystickCapabitlities.wUmax);
    stick->setStickRzRange(joystickCapabitlities.wVmin, joystickCapabitlities.wVmax);

    const int kMaxDegreesOfFreedom = 6;
    controls::Ranges outputLimits;

    for (auto i = 0; i < kMaxDegreesOfFreedom; ++i)
    {
        outputLimits.emplace_back(
            kDefaultStickLogicalRangeMin,
            kDefaultStickLogicalRangeMax);
    }

    stick->setOutputRanges(outputLimits);
    sticks.push_back(stick);

    joy->setSticks(sticks);
    joy->setDriver(this);

    return joy;
}

void MmWinDriver::notifyJoystickButtonsStateChanged(JoystickPtr& joy, DWORD buttonStates)
{
    DWORD currentButtonMask = 1;
    int currentButtonIndex = 0;

    if (!joy)
        return;

    while (currentButtonIndex < kMaxButtonNumber)
    {
        auto controlId = makeId(kButtonObjectType, currentButtonIndex);
        if (!joy->controlById(controlId))
            return;

        StateElement pressedState = (currentButtonMask & buttonStates) ? 1 : 0;
        State state;
        state.push_back(pressedState);
        joy->notifyControlStateChanged(controlId, state);
        ++currentButtonIndex;
        currentButtonMask *= 2;
    }
}

void MmWinDriver::notifyJoystickSticksStateChanged(JoystickPtr& joy, const JOYINFOEX& info)
{
    auto controlId = makeId(kStickObjectType, 0);
    if (!joy->controlById(controlId))
        return;

    State state;
    state.push_back(info.dwXpos);
    state.push_back(info.dwYpos);
    state.push_back(info.dwZpos);
    state.push_back(info.dwRpos);
    state.push_back(info.dwUpos);
    state.push_back(info.dwVpos);

    joy->notifyControlStateChanged(controlId, state);
}

QString MmWinDriver::makeId(const QString& objectType, uint objectIndex)
{
    return lit("%1:%2")
        .arg(objectType)
        .arg(objectIndex);
}

} // namespace driver
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx

#endif