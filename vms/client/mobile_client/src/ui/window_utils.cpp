#include "window_utils.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

#include <nx/utils/log/log.h>

QWindow *getMainWindow()
{
    QWindowList windows = qApp->topLevelWindows();
    if (windows.size() != 1)
        return nullptr;

    return windows.first();
}

#if !defined(Q_OS_IOS)
    QMargins getCustomMargins()
    {
        return QMargins();
    }

#endif

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

    void prepareWindow() {}

    void hideSystemUi() {}

    void showSystemUi() {}

    int statusBarHeight() {
        return 0;
    }

    int navigationBarHeight() {
        return 0;
    }

    bool isLeftSideNavigationBar()
    {
        return false;
    }

    bool isPhone() {
        return false;
    }

    void setKeepScreenOn(bool keepScreenOn) {
        Q_UNUSED(keepScreenOn)
    }

    void makeShortVibration()
    {
        NX_WARNING(NX_SCOPE_TAG, "Short vibration...");
    }

#endif // !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

#if !defined(Q_OS_ANDROID)

    void setScreenOrientation(Qt::ScreenOrientation orientation)
    {
        QWindow *window = getMainWindow();
        if (!window)
            return;

        window->reportContentOrientationChange(orientation);
    }

#endif // !defined(Q_OS_ANDROID)
