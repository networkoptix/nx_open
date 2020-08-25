#include "joystick_event_filter_win.h"

#if defined(Q_OS_WIN)

#include "mm_joystick_driver_win.h"

#include <Dbt.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace driver {

JoystickEventFilter::JoystickEventFilter(HWND windowId):
    m_windowId(windowId)
{
}

JoystickEventFilter::~JoystickEventFilter()
{
}

bool JoystickEventFilter::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
{
    auto winMessage = reinterpret_cast<MSG*>(message);
    auto mmDriver = MmWinDriver::instance();

    if (isJoystickConnectivityMessage(winMessage))
        mmDriver->notifyHardwareConfigurationChanged();

    return false;
}

bool JoystickEventFilter::isJoystickConnectivityMessage(MSG* message)
{
    auto messageType = message->message;

    if (messageType != WM_DEVICECHANGE)
        return false;

    auto changeType = message->wParam;

    bool hardwareConfigurationChanged = changeType == DBT_DEVNODES_CHANGED
        || changeType == DBT_DEVICEARRIVAL
        || changeType == DBT_DEVICEREMOVECOMPLETE;

    return hardwareConfigurationChanged;
}

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx

#endif // defined(Q_OS_WIN)