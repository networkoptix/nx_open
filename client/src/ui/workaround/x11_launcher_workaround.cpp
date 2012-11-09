#include "x11_launcher_workaround.h"
#include "x11_win_control.h"

#include <QEvent>

QnX11LauncherWorkaround::QnX11LauncherWorkaround():
    QObject(){
    hideLauncher();
}

QnX11LauncherWorkaround::~QnX11LauncherWorkaround() {
    restoreLauncher();
}

void QnX11LauncherWorkaround::hideLauncher() {
    x11_hideLauncher();
}

void QnX11LauncherWorkaround::restoreLauncher() {
    x11_showLauncher();
}

bool QnX11LauncherWorkaround::eventFilter(QObject *obj, QEvent *event) {
    switch (event->type()) {
    case QEvent::ApplicationActivate:
        hideLauncher();
        break;
    case QEvent::ApplicationDeactivate:
        restoreLauncher();
        break;
    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}
