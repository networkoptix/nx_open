// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QAbstractEventDispatcher>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QtMath>
#include <QtCore/QEventLoop>

#include <QtWidgets/QWidget>

#import <objc/runtime.h>
#import <Cocoa/Cocoa.h>

#import "mac_utils.h"

namespace {

NSString* fromQString(const QString& string)
{
    const auto utf8 = string.toUtf8();
    const auto cString = utf8.constData();
    return [[NSString alloc] initWithUTF8String:cString];
}

void saveFileBookmark(NSString* path)
{
    NSLog(@"saveFileBookmark(): %@", path);

    NSURL* url = [NSURL fileURLWithPath:path];

    NSError* error = nil;
    auto data = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
        includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
    if (error)
    {
        NSLog(@"Error securing bookmark for url %@ %@", url, error);
        return;
    }

    NSLog(@"Data length: %d", data.length);
    const auto prefs = [NSUserDefaults standardUserDefaults];
    const auto savedEntitlements = [prefs dictionaryForKey:@"directoryEntitlements"];

    const auto entitlements = [NSMutableDictionary dictionary];
    if (savedEntitlements)
    {
        NSLog(@"saveFileBookmark(): existing keys %@", entitlements.allKeys);
        [entitlements addEntriesFromDictionary:savedEntitlements];
    }

    [entitlements setObject:data forKey:path];
    [prefs setObject:entitlements forKey:@"directoryEntitlements"];
    [prefs synchronize];
}

} // namespace

bool mac_isSandboxed()
{
    NSDictionary* environ = [[NSProcessInfo processInfo] environment];
    return (nil != [environ objectForKey:@"APP_SANDBOX_CONTAINER_ID"]);
}

void mac_saveFileBookmark(const QString& qpath) {
    if (!mac_isSandboxed())
        return;

    NSString *path = fromQString(qpath);

    saveFileBookmark(path);
}

void mac_restoreFileAccess() {
    if (!mac_isSandboxed())
        return;

    NSArray *path = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    NSString *folder = [path objectAtIndex:0];
    NSLog(@"Your NSUserDefaults are stored in this folder: %@/Preferences", folder);

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSDictionary *entitlements = [prefs dictionaryForKey:@"directoryEntitlements"];
    if (!entitlements) {
        NSLog(@"mac_restoreFileAccess(): No directoryEntitlements");
        return;
    }

    NSLog(@"mac_restoreFileAccess(): Starting restoring access");
    for (NSData *data in entitlements.allValues) {
        NSError *error = nil;
        NSURL *url = [NSURL URLByResolvingBookmarkData:data options:NSURLBookmarkResolutionWithSecurityScope relativeToURL:nil bookmarkDataIsStale:nil error:&error];
        BOOL result = [url startAccessingSecurityScopedResource];
        NSLog(@"mac_restoreFileAccess(): Restored access for %@. Result: %d", url.path, result);
    }
}

void mac_stopFileAccess() {
    if (!mac_isSandboxed())
        return;

    NSUserDefaults* prefs = [NSUserDefaults standardUserDefaults];
    NSDictionary *entitlements = [prefs dictionaryForKey:@"directoryEntitlements"];
    if (!entitlements)
        return;

    for (NSData *data in entitlements.allValues) {
        NSError *error = nil;
        NSURL *url = [NSURL URLByResolvingBookmarkData:data options:NSURLBookmarkResolutionWithSecurityScope relativeToURL:nil bookmarkDataIsStale:nil error:&error];
        [url stopAccessingSecurityScopedResource];
        NSLog(@"mac_stopFileAccess(): Stopped access for %@", url.path);
    }
}

extern "C" {
void disable_animations(void *);
void enable_animations(void *);
}

id customWindowsToEnterFullScreenForWindow(id self, SEL _cmd, NSWindow *window) {
    return [NSArray arrayWithObject:window];
}

void setFullscreenTransitionHandler(QWidget* widget, TransitionStateCallback callback)
{
    NSWindow* nswindow = [reinterpret_cast<NSView *>(widget->winId()) window];

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserverForName:NSWindowWillEnterFullScreenNotification
        object:nswindow
        queue:nil
        usingBlock:^(NSNotification *) { callback(true); }];

    [defaultCenter addObserverForName:NSWindowWillExitFullScreenNotification
        object:nswindow
        queue:nil
        usingBlock:^(NSNotification *)
        {
            [nswindow setCollectionBehavior:NSWindowCollectionBehaviorDefault];
            callback(true);
        }];

    [defaultCenter addObserverForName:NSWindowDidEnterFullScreenNotification
        object:nswindow
        queue:nil
        usingBlock:^(NSNotification *) { callback(false); }];

    [defaultCenter addObserverForName:NSWindowDidExitFullScreenNotification
        object:nswindow
        queue:nil
        usingBlock:^(NSNotification *) { callback(false); }];
}

void mac_showFullScreen(void *winId, bool fullScreen) {
    NSView *nsview = (NSView *) winId;
    NSWindow *nswindow = [nsview window];

    bool isFullScreen = [nswindow styleMask] & NSFullScreenWindowMask;
    if (isFullScreen == fullScreen)
        return;

    if (fullScreen) {
        [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        [nswindow toggleFullScreen:nil];
    } else {
        [nswindow toggleFullScreen:nil];
        mac_disableFullscreenButton(winId);
    }
}

bool mac_isFullscreen(void *winId) {
    NSView *nsview = (NSView *) winId;
    NSWindow *nswindow = [nsview window];

    bool isFullScreen = [nswindow styleMask] & NSFullScreenWindowMask;
    return isFullScreen;
}

void mac_disableFullscreenButton(void *winId) {
    NSView *nsview = (NSView *) winId;
    NSWindow *nswindow = [nsview window];

    NSButton *button = [nswindow standardWindowButton:NSWindowFullScreenButton];
    if (button) {
        [button setHidden:YES];
        [button setEnabled:NO];
    }
}

void setHidesOnDeactivate(WId windowId, bool value)
{
    NSView* nativeView = reinterpret_cast<NSView*>(windowId);
    if (!nativeView)
        return;

    NSWindow* nativeWindow = [nativeView window];
    if (!nativeWindow)
        return;

    [nativeWindow setHidesOnDeactivate: value ? YES : NO];
}

void mac_setFullScreenAuxiliary(WId windowId)
{
    NSView* nativeView = reinterpret_cast<NSView*>(windowId);
    if (!nativeView)
        return;

    NSWindow* nativeWindow = [nativeView window];
    if (!nativeWindow)
        return;

    [nativeWindow setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
}
