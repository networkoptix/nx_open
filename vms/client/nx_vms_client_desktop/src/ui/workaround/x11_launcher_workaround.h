#ifndef X11_LAUNCHER_WORKAROUND_H
#define X11_LAUNCHER_WORKAROUND_H

#include <QtCore/QObject>

/**
 * This class implements a workaround for Linux X11 Gnome Launcher
 * that override fullscreen when it works in 3D mode (e.g. in Unity shell).
 * This class hides launcher when application activates and
 * resores launcher when application loses focus.
 */
class QnX11LauncherWorkaround: public QObject {
    Q_OBJECT

public:
    QnX11LauncherWorkaround();
    virtual ~QnX11LauncherWorkaround();

    static bool isUnity3DSession();

protected:
    bool eventFilter(QObject *, QEvent *);
private:
    void hideLauncher();
    void restoreLauncher();
};

#endif // X11_LAUNCHER_WORKAROUND_H
