#ifndef QN_SYSTEM_MENU_HOTKEY_H
#define QN_SYSTEM_MENU_HOTKEY_H

#include <QEvent>

/**
 * This class implements a workaround for QTBUG-806: 
 * 
 * Currently, Qt/Windows eats the Alt-Space key event unconditionally in 
 * QKeyMapperPrivate::translateKeyEvent. If the window however has no system menu 
 * (in which case the call to GetSystemMenu in qt_show_system_menu returns 0), 
 * then passing on the key event to the focus widget would allow the application 
 * to show a custom menu.
 */
class QnSystemMenuEvent: public QEvent {
public:
    /** Event type for system menu. */
    static const QEvent::Type SystemMenu = static_cast<QEvent::Type>(QEvent::User + 0x5481);

    QnSystemMenuEvent(): QEvent(SystemMenu) {}

    static void initialize();
};

#endif //QN_SYSTEM_MENU_HOTKEY_H
