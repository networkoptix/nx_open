// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_utils.h"

#include <UIKit/UIKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <WebKit/WKWebView.h>

#include <QtCore/QThread>
#include <QtGui/QColor>
#include <QtGui/QWindow>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>

#include <nx/utils/log/assert.h>

namespace {

void applyWebViewBackgroundColor(UIView* view, UIColor* color)
{
    if (![view isKindOfClass:[WKWebView class]])
    {
        for (UIView* subview in view.subviews)
            applyWebViewBackgroundColor(subview, color);
        return;
    }

    WKWebView* webView = (WKWebView*) view;
    webView.opaque = NO;
    webView.backgroundColor = color;
    webView.scrollView.backgroundColor = color;

    if (@available(iOS 15.0, *))
        webView.underPageBackgroundColor = color;
}

} // namespace

void prepareWindow()
{
    if (QWindow *window = getMainWindow())
    {
        window->setFlags(window->flags() | Qt::ExpandedClientAreaHint);
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

void setScreenOrientation(Qt::ScreenOrientation orientation)
{
    const bool portrait = orientation == Qt::PrimaryOrientation
        || orientation == Qt::PortraitOrientation
        || orientation == Qt::InvertedPortraitOrientation;

    if (@available(iOS 16.0, *))
    {
        const int orientationValue = portrait
            ? UIInterfaceOrientationMaskPortrait
            : UIInterfaceOrientationMaskLandscapeRight;
        UIWindowScene * windowScene = (UIWindowScene *)
                                     [[[UIApplication sharedApplication] connectedScenes] allObjects].firstObject;
        UIWindowSceneGeometryPreferencesIOS* perference = [[ UIWindowSceneGeometryPreferencesIOS alloc]
            initWithInterfaceOrientations: orientationValue];
        [windowScene requestGeometryUpdateWithPreferences:perference errorHandler: nil];
    }
    else
    {
        const int orientationValue = portrait
            ? UIDeviceOrientationPortrait
            : UIDeviceOrientationLandscapeRight;
        NSNumber *value = [NSNumber numberWithInt: orientationValue];
        [[UIDevice currentDevice] setValue:value forKey:@"orientation"];
        [UIViewController attemptRotationToDeviceOrientation];
    }
}

bool isPhone() {
    return UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone;
}

void setKeepScreenOn(bool keepScreenOn)
{
    const auto application = [UIApplication sharedApplication];
    [application setIdleTimerDisabled:(keepScreenOn ? YES : NO)];
}

void makeShortVibration()
{
    static constexpr int kShortVibrationId = 1519;
    AudioServicesPlaySystemSound(kShortVibrationId);
}

void setWebViewBackgroundColor(const QColor& color)
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread(), "Must run on the GUI thread"))
        return;

    UIColor* uiColor = [UIColor
        colorWithRed:color.redF()
        green:color.greenF()
        blue:color.blueF()
        alpha:color.alphaF()];

    for (QWindow* window: qApp->topLevelWindows())
    {
        if (!window->handle())
            continue;

        if (UIView* rootView = reinterpret_cast<UIView*>(window->winId()))
            applyWebViewBackgroundColor(rootView, uiColor);
    }
}

@interface QIOSViewController : UIViewController
@end

@implementation QIOSViewController (NxHideHomeIndicator)
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
@end
