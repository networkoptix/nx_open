#include "window_utils.h"

#include <UIKit/UIKit.h>
#include <QtGui/QWindow>

void prepareWindow()
{
    if (QWindow *window = getMainWindow())
    {
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

bool isLeftSideNavigationBar()
{
    return false;
}

bool isPhone() {
    return UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone;
}

void setKeepScreenOn(bool keepScreenOn) {
    [[UIApplication sharedApplication] setIdleTimerDisabled:(keepScreenOn ? YES : NO)];
}
