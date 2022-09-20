// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#ifdef Q_OS_MAC
    #include <ApplicationServices/ApplicationServices.h>
#endif

#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QWindow>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsObject>
#include <QtWidgets/QApplication>
#include <QtTest/QSpontaneKeyEvent>

namespace nx::vms::client::desktop::testkit::utils {

namespace {

/** Move OS cursor to specified position, requieres asking for permission on macOS. */
static void setMousePosition(QPoint screenPos)
{
    #ifdef Q_OS_MAC
        CFStringRef keys[] = { kAXTrustedCheckOptionPrompt };
        CFTypeRef values[] = { kCFBooleanTrue };
        CFDictionaryRef options = CFDictionaryCreate(
            nullptr,
            reinterpret_cast<const void **>(&keys),
            reinterpret_cast<const void **>(&values),
            sizeof(keys) / sizeof(keys[0]),
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

        if (AXIsProcessTrustedWithOptions(options))
            QCursor::setPos(screenPos);

        CFRelease(options);
    #else
        QCursor::setPos(screenPos);
    #endif
}

/** Send single key event. */
Qt::KeyboardModifiers sendKey(
    QObject* receiver,
    QString key,
    Qt::KeyboardModifiers modifiers,
    QEvent::Type eventType)
{
    int code;
    QString text;

    if (key.length() == 1)
    {
        static const QString codes =
            "01234567890-=!@#$%^&*()_+:;'\"\\[]{},./<>?`~QWERTYUIOPASDFGHJKLZXCVBNM";
        code = codes.contains(key.toUpper()) ? key.at(0).toUpper().unicode() : 0;
        text = key;
    }
    else
    {
        static QHash<QString, int> special({
            { "Alt", Qt::Key_Alt },
            { "Ctrl", Qt::Key_Control },
            { "Shift", Qt::Key_Shift },
            { "Escape", Qt::Key_Escape },
            { "Return", Qt::Key_Return },
            { "Enter", Qt::Key_Enter },
            { "Space", Qt::Key_Space },
            { "Backspace", Qt::Key_Backspace },
            { "Tab", Qt::Key_Tab },
            { "Meta", Qt::Key_Meta },
            { "Delete", Qt::Key_Delete },
            { "Left", Qt::Key_Left },
            { "Right", Qt::Key_Right },
            { "Up", Qt::Key_Up },
            { "Down", Qt::Key_Down },
            { "PgUp", Qt::Key_PageUp },
            { "PgDown", Qt::Key_PageDown },
            { "F1", Qt::Key_F1 },
            { "F2", Qt::Key_F2 },
            { "F3", Qt::Key_F3 },
            { "F4", Qt::Key_F4 },
            { "F5", Qt::Key_F5 },
            { "F6", Qt::Key_F7 },
            { "F7", Qt::Key_F7 },
            { "F8", Qt::Key_F8 },
            { "F9", Qt::Key_F9 },
            { "F10", Qt::Key_F10 },
            { "F11", Qt::Key_F11 },
            { "F12", Qt::Key_F12 },
        });

        static QHash<int, QString> specText({
            { Qt::Key_Space, " " },
            { Qt::Key_Return, "\r" },
            { Qt::Key_Enter, "\r" },
            { Qt::Key_Backspace, "\u007F" },
            { Qt::Key_Escape, "\u001B" }
        });

        code = special.value(key, 0);
        text = specText.value(code);
    }

    // QKeyEvent constructor automatically sets modifiers.
    QKeyEvent* event = new QKeyEvent(eventType, code, modifiers, text);
    QSpontaneKeyEvent::setSpontaneous(event);
    modifiers = event->modifiers();
    QCoreApplication::postEvent(receiver, event);

    return modifiers;
}

} // namespace

Q_INVOKABLE Qt::KeyboardModifiers sendKeys(
    QJSValue object,
    QString keys,
    KeyOption option,
    Qt::KeyboardModifiers modifiers)
{
    QObject* receiver = object.toQObject();

    if (!receiver)
        receiver = qGuiApp->focusWindow();

    if (!receiver)
        return modifiers;

    // QGraphicsObject should receive key events via its view.
    if (auto w = qobject_cast<QGraphicsObject*>(receiver))
    {
        auto views = w->scene()->views();
        for (int i = 0; i < views.size(); ++i)
        {
            QGraphicsView* view = views.at(i);
            if (!view->isVisible())
                continue;

            if (view->window()->windowHandle())
            {
                receiver = view;
                break;
            }
        }
    }

    const auto eventType = option == KeyRelease ? QEvent::KeyRelease : QEvent::KeyPress;

    for (int i = 0; i < keys.length(); ++i)
    {
        QStringList sequence;

        // Consume key sequence.
        if (keys.at(i) == '<')
        {
            // Find end of the sequence.
            const auto endi = keys.indexOf('>', i + 2);
            if (endi != -1)
            {
                sequence = keys.mid(i + 1, endi - i - 1).split('+');
                i = endi;
            }
            else
                sequence << keys.mid(i, 1); //< Single `<` key
        }
        else
            sequence << keys.mid(i, 1); //< Single key.

        // Simulate typing: first send key press event for each key in the sequence,
        // than send key release event, but in reverse.
        for (QString k: sequence)
            modifiers = sendKey(receiver, k, modifiers, eventType);

        if (option != KeyType)
            continue; //< Press or release has been sent.

        // Go through sequence in reverse
        auto iter = sequence.constEnd();
        while (iter != sequence.constBegin())
        {
            --iter;
            modifiers = sendKey(receiver, *iter, modifiers, QEvent::KeyRelease);
        }
    }

    return modifiers;
}

Qt::MouseButtons sendMouse(
    QWindow* windowHandle,
    QPoint screenPos,
    QString eventType,
    Qt::MouseButton button,
    Qt::KeyboardModifiers modifiers,
    Qt::MouseButtons buttons,
    bool nativeSetPos,
    QPoint pixelDelta,
    QPoint angleDelta,
    bool inverted)
{
    QPoint windowPos = windowHandle->mapFromGlobal(screenPos);
    QPoint localPos = windowPos;

    auto postEvent =
        [&](QEvent::Type type)
        {
            if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease)
                buttons.setFlag(button, type == QEvent::MouseButtonPress);

            QInputEvent* mappedEvent = nullptr;

            if (type == QEvent::Wheel)
            {
                mappedEvent = new QWheelEvent(
                    localPos, screenPos, pixelDelta, angleDelta, buttons, modifiers,
                    Qt::NoScrollPhase, inverted, Qt::MouseEventNotSynthesized);
            }
            else
            {
                mappedEvent = new QMouseEvent(
                    type, localPos, windowPos, screenPos, button, buttons, modifiers);
            }

            QSpontaneKeyEvent::setSpontaneous(mappedEvent);
            QCoreApplication::postEvent(windowHandle, mappedEvent);
        };

    if (nativeSetPos)
        setMousePosition(screenPos);

    if (eventType == "move")
    {
        postEvent(QEvent::MouseMove);
    }
    else if (eventType == "press")
    {
        postEvent(QEvent::MouseButtonPress);
    }
    else if (eventType == "release")
    {
        postEvent(QEvent::MouseButtonRelease);
    }
    else if (eventType == "click")
    {
        postEvent(QEvent::MouseButtonPress);
        postEvent(QEvent::MouseButtonRelease);
    }
    else if (eventType == "doubleclick")
    {
        postEvent(QEvent::MouseButtonPress);
        postEvent(QEvent::MouseButtonRelease);
        postEvent(QEvent::MouseButtonPress);
        postEvent(QEvent::MouseButtonRelease);

        postEvent(QEvent::MouseButtonDblClick);
    }
    else if (eventType == "wheel")
    {
        postEvent(QEvent::Wheel);
    }

    return buttons;
}

} // namespace nx::vms::client::desktop::testkit::utils
