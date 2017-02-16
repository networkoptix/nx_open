#pragma once

#if defined(Q_OS_WIN)

#include <QtCore/QAbstractNativeEventFilter>
#include <windows.h>

namespace nx {
namespace joystick {

class JoystickEventFilter: public QAbstractNativeEventFilter
{
public:
    JoystickEventFilter(HWND windowId);
    virtual ~JoystickEventFilter();
    virtual bool nativeEventFilter(const QByteArray &eventType, void* message, long* result) override;

private:
    bool isJoystickInteractionMessage(MSG* message, int* outJoystickNum);
    bool isJoystickConnectivityMessage(MSG* message);

private:
    HWND m_windowId;
};

} // namespace joystick
} // namespace nx

#endif // defined(Q_OS_WIN)