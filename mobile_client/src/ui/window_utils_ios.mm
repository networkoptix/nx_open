#include "window_utils.h"

#include <UIKit/UIKit.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

namespace {
    QWindow *topWindow() {
        QWindowList windows = qApp->topLevelWindows();
        if (windows.size() != 1)
            return nullptr;

        return windows.first();
    }
}

void prepareWindow() {
    if (QWindow *window = topWindow()) {
        window->setFlags(window->flags() | Qt::MaximizeUsingFullscreenGeometryHint);
        window->showMaximized();
    }
    [UIApplication sharedApplication].statusBarStyle = UIStatusBarStyleLightContent;

}

void hideSystemUi() {
    [UIApplication sharedApplication].statusBarHidden = YES;
}

void showSystemUi() {
    [UIApplication sharedApplication].statusBarHidden = NO;
}

int statusBarHeight() {
    CGSize size = [[UIApplication sharedApplication] statusBarFrame].size;
    return qMin(size.width, size.height);
}

int navigationBarHeight() {
    return 0;
}

bool isPhone() {
    return UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone;
}
