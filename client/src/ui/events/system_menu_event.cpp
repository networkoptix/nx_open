#include "system_menu_event.h"
#include <QAbstractEventDispatcher>
#include <QWidget>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <Windows.h>

namespace {

    QAbstractEventDispatcher::EventFilter qn_sysMenu_oldEventFilter;

    bool qn_sysMenu_eventFilter(void *message) {
        MSG *msg = static_cast<MSG *>(message);

        if(msg->message == WM_SYSKEYDOWN && msg->wParam == ' ') {
            bool altPressed = (GetKeyState(VK_LMENU) & 0x80) || (GetKeyState(VK_RMENU) & 0x80);
            if(altPressed) {
                QWidget *widget = QWidget::find(msg->hwnd);
                if(widget)
                    QCoreApplication::postEvent(widget->window(), new QnSystemMenuEvent());
                /* It is important not to return true here as it may mess up the QKeyMapper. */
            }
        }

        if(qn_sysMenu_oldEventFilter) {
            return qn_sysMenu_oldEventFilter(message);
        } else {
            
            return false;
        }
    }

    Q_GLOBAL_STATIC_WITH_INITIALIZER(bool, qn_sysMenu_initEventFilter, {
        qn_sysMenu_oldEventFilter = QAbstractEventDispatcher::instance(qApp->thread())->setEventFilter(&qn_sysMenu_eventFilter);
    });

} // anonymous namespace

#endif // Q_OS_WIN


void QnSystemMenuEvent::initialize() {
#ifdef Q_OS_WIN
    qn_sysMenu_initEventFilter();
#endif
}

