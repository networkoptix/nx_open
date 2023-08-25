// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"
#include "windows.h"

#include <QtWidgets/QApplication>

#include <range/v3/view/reverse.hpp>

namespace nx::vms::client::desktop::testkit::utils {

namespace {

USHORT getKeyCode(const QString& key)
{
    static const QHash<QString, int> specialKeys({
        {"Alt", VK_MENU},
        {"Ctrl", VK_CONTROL},
        {"Shift", VK_SHIFT},
        {"Escape", VK_ESCAPE},
        {"Return", VK_RETURN},
        {"Enter", VK_RETURN},
        {"Space", VK_SPACE},
        {"Backspace", VK_BACK},
        {"Tab", VK_TAB},
        {"Delete", VK_DELETE},
        {"Left", VK_LEFT},
        {"Right", VK_RIGHT},
        {"Up", VK_UP},
        {"Down", VK_DOWN},
        {"PgUp", VK_PRIOR},
        {"PgDown", VK_NEXT},
        {"F1", VK_F1},
        {"F2", VK_F2},
        {"F3", VK_F3},
        {"F4", VK_F4},
        {"F5", VK_F5},
        {"F6", VK_F6},
        {"F7", VK_F7},
        {"F8", VK_F8},
        {"F9", VK_F9},
        {"F10", VK_F10},
        {"F11", VK_F11},
        {"F12", VK_F12}
     });
    return specialKeys.value(key, key.at(0).toUpper().unicode());
};

/**
* Send single key event.
* To send a capital letter or some special symbol here needed to emulate Shift press as well.
* But logically only one key will be presses.
*/
void sendKey(const QString& key)
{
    wchar_t buff[120] = {0};
    std::vector<INPUT> keyQueue;
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ILANGUAGE, buff, sizeof(buff));
    const auto keyboardLayout = LoadKeyboardLayout(buff, KLF_ACTIVATE);

    const auto vk = VkKeyScanEx(key.toLocal8Bit().data()[0], keyboardLayout);
    const USHORT scanCode = MapVirtualKey(LOBYTE(vk), 0);
    const USHORT shiftScanCode = MapVirtualKey(VK_LSHIFT, 0);

    if (HIBYTE(vk) == 1) //< Check if the Shift key should be pressed for this key sequence.
    {
        // Press Shift key.
        keyQueue.push_back({
            .type = INPUT_KEYBOARD,
            .ki = {
                .wScan = shiftScanCode,
                .dwFlags = KEYEVENTF_SCANCODE
            },
        });
    }

    // KeyDown.
    keyQueue.push_back({
        .type = INPUT_KEYBOARD,
        .ki = {
            .wScan = scanCode,
            .dwFlags = KEYEVENTF_SCANCODE
        },
    });

    // KeyUp.
    keyQueue.push_back({
        .type = INPUT_KEYBOARD,
        .ki = {
            .wScan = scanCode,
            .dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP
        },
    });

    if (HIBYTE(vk) == 1) //<Release if previouly pressed.
    {
        // Release Shift key.
        keyQueue.push_back({
            .type = INPUT_KEYBOARD,
            .ki = {
                .wScan = shiftScanCode,
                .dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP
            },
        });
    }

    if (keyboardLayout)
        UnloadKeyboardLayout(keyboardLayout);

    ::SendInput(keyQueue.size(), keyQueue.data(), sizeof(INPUT));
}

void sendKeySequence(const QStringList& keyList, const KeyOption option)
{
    std::vector<INPUT> keyQueue = {};

    for (const auto key: keyList)
    {
        keyQueue.push_back({
            .type = INPUT_KEYBOARD,
            .ki = {
                .wVk = getKeyCode(key),
                .dwFlags = option == KeyRelease ? KEYEVENTF_KEYUP: 0U,
            }
        });
    }

    if (option == KeyType)
    {
        for (auto key : ranges::reverse_view(keyList))
        {
            keyQueue.push_back({
                .type = INPUT_KEYBOARD,
                .ki = {
                    .wVk = getKeyCode(key),
                    .dwFlags = KEYEVENTF_KEYUP
                }
            });
        }
    }
    ::SendInput(keyQueue.size(), keyQueue.data(), sizeof(INPUT));
}

void callSendMouse(
    const QPoint screenPos,
    const QString eventType,
    const Qt::MouseButton button = Qt::LeftButton,
    const DWORD scrollDelta = 0)
{
    LONG x_coord = screenPos.x() * 65536 / GetSystemMetrics(SM_CXSCREEN);
    LONG y_coord = screenPos.y() * 65536 / GetSystemMetrics(SM_CYSCREEN);

    if (eventType == "move")
    {
        INPUT mouseEvent = {
            .type = INPUT_MOUSE,
            .mi = {
                .dx = x_coord,
                .dy = y_coord,
                .dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
            }
        };
        ::SendInput(1, &mouseEvent, sizeof(INPUT));
    }
    else if (eventType == "click")
    {
        INPUT mouseEvent = {
            .type = INPUT_MOUSE,
            .mi = {
                .dx = x_coord,
                .dy = y_coord,
                .dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
            }
        };
        mouseEvent.mi.dwFlags |= button == Qt::LeftButton ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
        ::SendInput(1, &mouseEvent, sizeof(INPUT));
        ::Sleep(50);

        mouseEvent.mi.dwFlags = button == Qt::LeftButton ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
        ::SendInput(1, &mouseEvent, sizeof(INPUT));
    }
    else if (eventType == "press")
    {
        INPUT mouseEvent = {
            .type = INPUT_MOUSE,
            .mi = {
                .dx = x_coord,
                .dy = y_coord,
                .dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
            }
        };
        mouseEvent.mi.dwFlags |= button == Qt::LeftButton ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
        ::SendInput(1, &mouseEvent, sizeof(INPUT));
    }
    else if (eventType == "release")
    {
        INPUT mouseEvent = {
            .type = INPUT_MOUSE,
            .mi = {
                .dx = x_coord,
                .dy = y_coord,
                .dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
            }
        };
        mouseEvent.mi.dwFlags |= button == Qt::LeftButton ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
        ::SendInput(1, &mouseEvent, sizeof(INPUT));
    }
    else if (eventType == "wheel")
    {
        INPUT mouseEvent = {
            .type = INPUT_MOUSE,
            .mi = {
                .dx = x_coord,
                .dy = y_coord,
                .mouseData = scrollDelta,
                .dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_WHEEL,
            }
        };
        ::SendInput(1, &mouseEvent, sizeof(INPUT));
    }
    ::Sleep(10);
}

} // namespace

Q_INVOKABLE Qt::KeyboardModifiers sendKeys(
    QString keys,
    QJSValue object,
    KeyOption option,
    Qt::KeyboardModifiers modifiers)
{
    qGuiApp->focusWindow();

    for (int i = 0; i < keys.length(); ++i)
    {
        // Consume key sequence.
        if (keys.at(i) == '<')
        {
            // Find end of the sequence.
            const auto endi = keys.indexOf('>', i + 2);
            if (endi != -1)
            {
                const QStringList sequence = keys.mid(i + 1, endi - i - 1).split('+');
                i = endi;
                sendKeySequence(sequence, option);
                continue;
            }
        }
        sendKey(keys.mid(i, 1));
    }
    return Qt::NoModifier;
}

Qt::MouseButtons sendMouse(
    QPoint screenPos,
    QString eventType,
    Qt::MouseButton button,
    Qt::MouseButtons buttons,
    Qt::KeyboardModifiers modifiers,
    QWindow* windowHandle,
    bool nativeSetPos,
    QPoint pixelDelta,
    QPoint angleDelta,
    bool inverted,
    int scrollDelta)
{
    QString keyModifier;
    if (modifiers == Qt::ShiftModifier)
        keyModifier = "Shift";
    else if (modifiers == Qt::ControlModifier)
        keyModifier = "Ctrl";
    else if (modifiers == Qt::AltModifier)
        keyModifier = "Alt";

    if (modifiers != Qt::NoModifier)
        sendKeySequence({ keyModifier }, KeyPress);

    if (eventType == "doubleclick")
    {
        callSendMouse(screenPos, "click");
        callSendMouse(screenPos, "click");
    }
    else
    {
        callSendMouse(screenPos, eventType, button, scrollDelta);
    }

    if (modifiers != Qt::NoModifier)
        sendKeySequence({ keyModifier }, KeyRelease);

    return Qt::NoButton;
}

} // namespace nx::vms::client::desktop::testkit::utils
