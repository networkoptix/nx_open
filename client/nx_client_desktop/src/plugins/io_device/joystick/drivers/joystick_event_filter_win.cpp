#if defined(Q_OS_WIN)

#include "joystick_event_filter_win.h"
#include "mm_joystick_driver_win.h"

#include <WinUser.h>
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

bool JoystickEventFilter::nativeEventFilter(const QByteArray &eventType, void* message, long* result)
{
    auto winMessage = reinterpret_cast<MSG*>(message); 

    auto winMessageType = winMessage->message;
    int joystickIndex = -1;

    auto mmDriver = MmWinDriver::instance();

    if (isJoystickInteractionMessage(winMessage, &joystickIndex))
    {
        if (joystickIndex < 0)
            return false;

        JOYINFOEX info; 
        info.dwSize = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNALL;

        auto result  = joyGetPosEx(0, &info);

        if (result != JOYERR_NOERROR)
            return false;

        mmDriver->notifyJoystickStateChanged(info, joystickIndex);
    }

    if (isJoystickConnectivityMessage(winMessage))    
        mmDriver->notifyHardwareConfigurationChanged();        

    return false;
}

bool JoystickEventFilter::isJoystickInteractionMessage(MSG* message, int* outJoystickNum)
{
    *outJoystickNum = -1;

    auto messageType = message->message;
    
    if (messageType == MM_JOY1MOVE 
        || messageType == MM_JOY1BUTTONDOWN 
        || messageType == MM_JOY1BUTTONUP)
    {
        *outJoystickNum = 0;
        return true;
    }

    if (messageType == MM_JOY2MOVE
        || messageType == MM_JOY2BUTTONDOWN
        || messageType == MM_JOY2BUTTONUP)
    {
        *outJoystickNum = 1;
        return true;
    }

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