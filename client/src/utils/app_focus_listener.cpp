#include "app_focus_listener.h"
#include "x11_win_control.h"

#include <QEvent>

QnAppFocusListener::QnAppFocusListener():
    QObject(){
    hideLauncher();
}

QnAppFocusListener::~QnAppFocusListener(){
    restoreLauncher();
}

void QnAppFocusListener::hideLauncher(){
    x11_hideLauncher();
}

void QnAppFocusListener::restoreLauncher(){
    x11_showLauncher();
}

bool QnAppFocusListener::eventFilter(QObject *obj, QEvent *event){
#ifdef Q_WS_X11
    switch (event->type()){
    case QEvent::ApplicationActivate:
        hideLauncher();
        break;
    case QEvent::ApplicationDeactivate:
        restoreLauncher();
        break;
    default:
        break;
    }
#endif
    return QObject::eventFilter(obj, event);
}
