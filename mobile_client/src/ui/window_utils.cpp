#include "window_utils.h"

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

bool isPhone() {
    return false;
}

#endif
