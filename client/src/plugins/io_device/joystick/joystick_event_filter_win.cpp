#if defined(Q_OS_WIN)

#include "joystick_event_filter_win.h"
#include "mm_joystick_driver_win.h"

#include <WinUser.h>
#include <Dbt.h>

namespace nx {
namespace joystick {

JoystickEventFilter::JoystickEventFilter(HWND windowId):
    m_windowId(windowId)
{
    auto joystickCount = joyGetNumDevs();

    qDebug() << "=======> Got joystick count: " << joystickCount;

    if (joystickCount <= 0)
        qDebug() << "=======> Joysticks not found:" << joystickCount;

    for (auto i = 0; i < joystickCount; ++i)
    {
        JOYCAPS capabilities;
        auto result = joyGetDevCaps(0, &capabilities, sizeof(capabilities));

        if (result != JOYERR_NOERROR)
            qDebug() << "========> ERROR WHILE RETREIVING JOSTICK CAPABILITIES";

        qDebug() << "=====> Manufacturer Id" << capabilities.wMid;
        qDebug() << "=====> Product Id" << capabilities.wPid;
        qDebug() << "=====> Product name" << QString::fromWCharArray(capabilities.szPname);
        qDebug() << "=====> Corrdinate limits:"
            << "x:" << capabilities.wXmin << capabilities.wXmax
            << "y:" << capabilities.wYmin << capabilities.wYmax
            << "z:" << capabilities.wZmin << capabilities.wZmax;

        qDebug() << "======> Number of buttons" << capabilities.wNumButtons;
        qDebug() << "-------------------------------------------------------";
        qDebug() << "                                                       ";
    }

    MMRESULT result = JOYERR_NOERROR;
    for (auto i = 0; i < 16; ++i)
    {
        JOYINFO info;
        if (joyGetPos(i, &info) != JOYERR_NOERROR)
        {
            qDebug() << "Joystick" << i << "is unplugged";
            continue;
        }

        result = joySetCapture(m_windowId, i, 100, false);

        if (result != JOYERR_NOERROR)
            qDebug() << "========> ERROR WHILE CAPTURING JOYSTICK" << i;

        result = joySetThreshold(i, 10);
    }

    if (result != JOYERR_NOERROR)
        qDebug() << "========> ERROR WHILE CAPTURING JOYSTICK";

    qDebug() << "==========> Joystick is captured";
}   

JoystickEventFilter::~JoystickEventFilter()
{
    joyReleaseCapture(0);
}

bool JoystickEventFilter::nativeEventFilter(const QByteArray &eventType, void* message, long* result)
{
    auto winMessage = reinterpret_cast<MSG*>(message); 

    auto winMessageType = winMessage->message;
    int joystickIndex = -1;

    if (isJoystickInteractionMessage(winMessage, &joystickIndex))
    {
        if (joystickIndex < 0)
            return false;

        JOYINFOEX info; 
        info.dwSize = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNALL;

        auto result  = joyGetPosEx(0, &info);

        if (result != JOYERR_NOERROR)
            qDebug() << "===>> ERROR!!!!!";

        auto mmDriver = nx::joystick::driver::MmWinDriver::instance();
        mmDriver->updateJoystickState(info, joystickIndex);

        //qDebug() << "JOSTICK POSITION NOW: " << info.dwXpos << info.dwYpos << info.dwZpos << info.dwUpos << info.dwRpos << info.dwButtons << info.dwPOV <<info.dwVpos <<info.dwReserved1 << info.dwReserved2;
    }
    

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

    if (changeType == DBT_DEVICEARRIVAL)
    {
        // TODO #dmishin do something when joystick is connected.   
    }

    if (changeType == DBT_DEVICEREMOVECOMPLETE)
    {
        // TODO #dmishinn do something when joystick is disconnected.
    }

    return false;

}

} // namespace joystick
} // namespace nx

#endif // defined(Q_OS_WIN)