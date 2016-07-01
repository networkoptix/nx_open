#include "window_utils.h"

#ifdef Q_OS_ANDROID

#include <QtAndroidExtras/QAndroidJniObject>
#include <QtGui/QGuiApplication>

namespace {

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

#endif
