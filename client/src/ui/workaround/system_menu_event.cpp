#include "system_menu_event.h"

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QWidget>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <utils/qt5port_windows_specific.h>

namespace {
    //TODO: #Elric #QT5PORT - check correctness please --gdm
    class QnSystemMenuEventFilter: public QAbstractNativeEventFilter {

        virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE {
            Q_UNUSED(eventType);
            Q_UNUSED(result);

            MSG *msg = static_cast<MSG *>(message);

            if(msg->message == WM_SYSKEYDOWN && msg->wParam == ' ') {
                bool altPressed = (GetKeyState(VK_LMENU) & 0x80) || (GetKeyState(VK_RMENU) & 0x80);
                if(altPressed) {
                    QWidget *widget = QWidget::find(hwndToWid(msg->hwnd));
                    if(widget)
                        QCoreApplication::postEvent(widget->window(), new QnSystemMenuEvent());
                    /* It is important not to return true here as it may mess up the QKeyMapper. */
                }
            }
            return false;
        }

    };

    QnSystemMenuEventFilter* sysMenu_eventFilter = NULL;

} // anonymous namespace
#endif // Q_OS_WIN


void QnSystemMenuEvent::initialize() {
#ifdef Q_OS_WIN
    if(sysMenu_eventFilter)
        return;

    sysMenu_eventFilter = new QnSystemMenuEventFilter();
    QAbstractEventDispatcher::instance(qApp->thread())->installNativeEventFilter(sysMenu_eventFilter);
#endif
}


void QnSystemMenuEvent::deinitialize() {
#ifdef Q_OS_WIN
    if(sysMenu_eventFilter == NULL)
        return;

    delete sysMenu_eventFilter;
    sysMenu_eventFilter = NULL;
#endif
}
