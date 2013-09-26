#include "system_menu_event.h"

#ifdef Q_OS_WIN

#include <Windows.h>

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QWidget>

#include <utils/common/warnings.h>

namespace {
    class QnSystemMenuEventFilter: public QObject, public QAbstractNativeEventFilter {
    public:
        QnSystemMenuEventFilter(QObject *parent = NULL): QObject(parent) {}

        virtual bool nativeEventFilter(const QByteArray &, void *message, long *) override {
            MSG *msg = static_cast<MSG *>(message);

            if(msg->message == WM_SYSKEYDOWN && msg->wParam == ' ') {
                bool altPressed = (GetKeyState(VK_LMENU) & 0x80) || (GetKeyState(VK_RMENU) & 0x80);
                if(altPressed) {
                    QWidget *widget = QWidget::find(reinterpret_cast<WId>(msg->hwnd));
                    if(widget)
                        QCoreApplication::postEvent(widget->window(), new QnSystemMenuEvent());
                    /* It is important not to return true here as it may mess up the QKeyMapper. */
                }
            }

            return false;
        }
    };

} // anonymous namespace

void QnSystemMenuEvent::initialize() {
    if(!qApp) {
        qnWarning("QApplication instance must exist before system menu event filter is initialized.");
        return;
    }

    qApp->installNativeEventFilter(new QnSystemMenuEventFilter(qApp));
}

#else // Q_OS_WIN

void QnSystemMenuEvent::initialize() {}

#endif // Q_OS_WIN

