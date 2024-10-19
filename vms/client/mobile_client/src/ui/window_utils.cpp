// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_utils.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtCore/QLocale>

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

    void setScreenOrientation(Qt::ScreenOrientation orientation)
    {
        QWindow *window = getMainWindow();
        if (!window)
            return;

        window->reportContentOrientationChange(orientation);
    }

#endif // !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

#if !defined(Q_OS_ANDROID)

    bool is24HoursTimeFormat()
    {
        const auto format = QLocale::system().timeFormat();
        return !format.contains("AP", Qt::CaseInsensitive);
    }

    int androidKeyboardHeight()
    {
        return 0;
    }

    void requestRecordAudioPermissionIfNeeded()
    {
    }

    void setAndroidGestureExclusionArea(int startY, int height)
    {
    }

#endif // !defined(Q_OS_ANDROID)

#if !defined(Q_OS_IOS)

QString webkitUrl()
{
    return {};
}

#endif
