#include "app_focus_listener.h"
#include "wmctrl.h"

#include <QEvent>

void QnAppFocusListener::hideLauncher(){
    execute_wmctrl(QLatin1String("wmctrl -r launcher -b add,below"));
}

void QnAppFocusListener::restoreLauncher(){
    execute_wmctrl(QLatin1String("wmctrl -r launcher -b remove,below"));
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
