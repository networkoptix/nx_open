#include "window_utils.h"

#ifdef Q_OS_ANDROID

#include <QtAndroidExtras/QAndroidJniObject>
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

const char* kUtilsClass = "com/networkoptix/nxwitness/utils/QnWindowUtils";

} // anonymous namespace

void prepareWindow()
{
    QAndroidJniObject::callStaticMethod<void>(kUtilsClass, "prepareSystemUi");
}

void hideSystemUi()
{
    QAndroidJniObject::callStaticMethod<void>(kUtilsClass, "hideSystemUi");
}

void showSystemUi()
{
    QAndroidJniObject::callStaticMethod<void>(kUtilsClass, "showSystemUi");
}

int statusBarHeight()
{
    const int size = QAndroidJniObject::callStaticMethod<jint>(kUtilsClass, "getStatusBarHeight");
    return static_cast<int>(size / qApp->devicePixelRatio());
}

bool isLeftSideNavigationBar()
{
    return QAndroidJniObject::callStaticMethod<jboolean>(kUtilsClass, "isLeftSideNavigationBar");
}

int navigationBarHeight()
{
    const int size = QAndroidJniObject::callStaticMethod<jint>(kUtilsClass, "getNavigationBarHeight");
    return static_cast<int>(size / qApp->devicePixelRatio());
}

bool isPhone()
{
    return QAndroidJniObject::callStaticMethod<jboolean>(kUtilsClass, "isPhone");
}

void setKeepScreenOn(bool keepScreenOn)
{
    QAndroidJniObject::callStaticMethod<void>(kUtilsClass, "setKeepScreenOn", "(Z)V", keepScreenOn);
}

void setScreenOrientation(Qt::ScreenOrientation orientation)
{
    QAndroidJniObject::callStaticMethod<void>(kUtilsClass, "setScreenOrientation", "(I)V",
        androidOrientationFromQtOrientation(orientation));
}

#endif
