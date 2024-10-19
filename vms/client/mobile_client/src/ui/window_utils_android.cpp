// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_utils.h"

#ifdef Q_OS_ANDROID

#include <QtCore/QJniObject>
#include <QtGui/QGuiApplication>

namespace {

enum class AndroidScreenOrientation
{
    unspecified = -1,
    landscape = 0x0b,
    portrait = 0x0c,
    reverseLandscape = 0x08,
    reversePortrait = 0x09,
};

AndroidScreenOrientation androidOrientationFromQtOrientation(Qt::ScreenOrientation orientation)
{
    switch (orientation)
    {
        case Qt::PrimaryOrientation:
            return AndroidScreenOrientation::unspecified;
        case Qt::PortraitOrientation:
            return AndroidScreenOrientation::portrait;
        case Qt::LandscapeOrientation:
            return AndroidScreenOrientation::landscape;
        case Qt::InvertedPortraitOrientation:
            return AndroidScreenOrientation::reversePortrait;
        case Qt::InvertedLandscapeOrientation:
            return AndroidScreenOrientation::reverseLandscape;
        default:
            return AndroidScreenOrientation::unspecified;
    }
}

const char* kUtilsClass = "com/nxvms/mobile/utils/QnWindowUtils";

} // anonymous namespace

void prepareWindow()
{
    QJniObject::callStaticMethod<void>(kUtilsClass, "prepareSystemUi");
}

void hideSystemUi()
{
    QJniObject::callStaticMethod<void>(kUtilsClass, "hideSystemUi");
}

void showSystemUi()
{
    QJniObject::callStaticMethod<void>(kUtilsClass, "showSystemUi");
}

int statusBarHeight()
{
    const int size = QJniObject::callStaticMethod<jint>(kUtilsClass, "getStatusBarHeight");
    return static_cast<int>(size / qApp->devicePixelRatio());
}

bool isPhone()
{
    return QJniObject::callStaticMethod<jboolean>(kUtilsClass, "isPhone");
}

void setKeepScreenOn(bool keepScreenOn)
{
    QJniObject::callStaticMethod<void>(kUtilsClass, "setKeepScreenOn", "(Z)V", keepScreenOn);
}

void setScreenOrientation(Qt::ScreenOrientation orientation)
{
    QJniObject::callStaticMethod<void>(kUtilsClass, "setScreenOrientation", "(I)V",
        androidOrientationFromQtOrientation(orientation));
}

void makeShortVibration()
{
    QJniObject::callStaticMethod<void>(kUtilsClass, "makeShortVibration");
}

bool is24HoursTimeFormat()
{
    return QJniObject::callStaticMethod<jboolean>(kUtilsClass, "is24HoursTimeFormat");
}

void setAndroidGestureExclusionArea(int startY, int height)
{
    QJniObject::callStaticMethod<void>(
        kUtilsClass,
        "setGestureExclusionArea",
        "(II)V",
        startY, height);
}

int androidKeyboardHeight()
{
    const int size = QJniObject::callStaticMethod<jint>(kUtilsClass, "keyboardHeight");
    return static_cast<int>(size / qApp->devicePixelRatio());
}

void requestRecordAudioPermissionIfNeeded()
{
    QJniObject::callStaticMethod<void>(kUtilsClass, "requestRecordAudioPermissionIfNeeded");
}

#endif
