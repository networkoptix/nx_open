#include "x11_launcher_workaround.h"
#include "x11_win_control.h"

#include <QtCore/QEvent>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

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

bool QnX11LauncherWorkaround::isUnity3DSession() {
#ifdef Q_OS_LINUX
    /* This function assumes that unity session sets environment variable
       XDG_CURRENT_DESKTOP to 'Unity', has running process whose name is unity-panel-service
       and hasn't process unity-2d-panel. */

    if (qgetenv("XDG_CURRENT_DESKTOP") != "Unity")
        return false;

    /* There is appeared case to disable fullscreen in Unity-2D too. We'll enable it later. */
    return true;

    bool hasUnityPanelService = false;

    QDir procDir(lit("/proc"));
    QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &procEntry, entries) {
        QFileInfo info(lit("/proc/") + procEntry + lit("/exe"));
        QString realPath = info.symLinkTarget();
        if (realPath.endsWith(lit("unity-panel-service")))
            hasUnityPanelService = true;
        if (realPath.endsWith(lit("unity-2d-panel")))
            return false;
    }
    return hasUnityPanelService;
#endif
    return false;
}
