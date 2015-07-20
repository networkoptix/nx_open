#include "window_utils.h"

#ifdef Q_OS_ANDROID

#include <QtAndroidExtras/QAndroidJniObject>

void prepareWindow() {
    QAndroidJniObject::callStaticMethod<void>("com/networkoptix/nxwitness/utils/QnWindowUtils", "prepareSystemUi");
}

void hideSystemUi() {
    QAndroidJniObject::callStaticMethod<void>("com/networkoptix/nxwitness/utils/QnWindowUtils", "hideSystemUi");
}

void showSystemUi() {
    QAndroidJniObject::callStaticMethod<void>("com/networkoptix/nxwitness/utils/QnWindowUtils", "showSystemUi");
}

int statusBarHeight() {
    return QAndroidJniObject::callStaticMethod<jint>("com/networkoptix/nxwitness/utils/QnWindowUtils", "getStatusBarHeight");
}

int navigationBarHeight() {
    return QAndroidJniObject::callStaticMethod<jint>("com/networkoptix/nxwitness/utils/QnWindowUtils", "getNavigationBarHeight");
}

bool isTablet() {
    return QAndroidJniObject::callStaticMethod<jboolean>("com/networkoptix/nxwitness/utils/QnWindowUtils", "isTablet");
}

#endif
