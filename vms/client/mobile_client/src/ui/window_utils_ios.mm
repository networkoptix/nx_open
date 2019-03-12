#include "window_utils.h"

#include <UIKit/UIKit.h>
#include <QtGui/QWindow>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>

#include <AudioToolbox/AudioToolbox.h>

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

int statusBarHeight()
{
    const auto orientation = qApp->primaryScreen()->orientation();
    const bool isLandscape = orientation == Qt::LandscapeOrientation
        || orientation == Qt::InvertedLandscapeOrientation;

    // iOS phone does not have status bar in landscape mode
    if (isPhone() && isLandscape)
        return 0;

    CGSize size = [[UIApplication sharedApplication] statusBarFrame].size;
    return qMin(size.width, size.height);
}

int navigationBarHeight()
{
    return 0;
}

bool isLeftSideNavigationBar()
{
    return false;
}

QMargins getCustomMargins()
{
    const auto windowId = getMainWindow()->winId();
    const auto nativeView = reinterpret_cast<UIView*>(windowId);
    if (!nativeView)
        return QMargins();

    if (@available(iOS 11, *))
    {
        auto safeInsets = [nativeView safeAreaInsets];
        return QMargins(safeInsets.left, safeInsets.top, safeInsets.right, safeInsets.bottom);
    }
    else
    {
        return QMargins();
    }
}

bool isPhone() {
    return UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone;
}

void setKeepScreenOn(bool keepScreenOn) {
    [[UIApplication sharedApplication] setIdleTimerDisabled:(keepScreenOn ? YES : NO)];
}

void makeShortVibration()
{
    static constexpr int kShortVibrationId = 1519;
    AudioServicesPlaySystemSound(kShortVibrationId);
}
