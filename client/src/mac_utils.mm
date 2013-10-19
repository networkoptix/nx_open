#import "mac_utils.h"

#import <objc/runtime.h>
#import <Cocoa/Cocoa.h>

extern "C" {
void disable_animations(void *);
void enable_animations(void *);
}

id customWindowsToEnterFullScreenForWindow(id self, SEL _cmd, NSWindow *window) {
    return [NSArray arrayWithObject:window];
}

void mac_initFullScreen(void *winId, void *qnmainwindow) {
    NSView *nsview = (NSView *) winId;
    NSWindow *nswindow = [nsview window];

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserverForName:NSWindowWillEnterFullScreenNotification
                                      object:nswindow
                                      queue:nil
                                      usingBlock:^(NSNotification *) {
                                           disable_animations(qnmainwindow);
                                      }];

    [defaultCenter addObserverForName:NSWindowWillExitFullScreenNotification
                                      object:nswindow
                                      queue:nil
                                      usingBlock:^(NSNotification *) {
                                           disable_animations(qnmainwindow);
                                      }];
    [defaultCenter addObserverForName:NSWindowDidEnterFullScreenNotification
                                      object:nswindow
                                      queue:nil
                                      usingBlock:^(NSNotification *) {
                                           enable_animations(qnmainwindow);
                                      }];

    [defaultCenter addObserverForName:NSWindowDidExitFullScreenNotification
                                      object:nswindow
                                      queue:nil
                                      usingBlock:^(NSNotification *) {
                                           enable_animations(qnmainwindow);
                                      }];
    [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

//    Class xclass = NSClassFromString(@"QNSWindowDelegate");
//    class_replaceMethod(xclass, @selector(customWindowsToEnterFullScreenForWindow:), (IMP) customWindowsToEnterFullScreenForWindow, "@@:@");
//    class_replaceMethod(xclass, @selector(windowDidExitFullScreen:), (IMP) windowSwitchedFullScreen, "@@:@");

//    [nswindow.delegate respondsToSelector: @selector(customWindowsToEnterFullScreenForWindow:)
//            withKey:nil
//            usingBlock:^(NSWindow *window) {
//                int i = 1;
//                return nil;
//        }
//        ];
}

void mac_showFullScreen(void *winId, bool fullScreen) {
    NSView *nsview = (NSView *) winId;
    NSWindow *nswindow = [nsview window];

    bool isFullScreen = [nswindow styleMask] & NSFullScreenWindowMask;
    if (isFullScreen == fullScreen)
        return;

    if (fullScreen) {
//        [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        [nswindow toggleFullScreen:nil];
    } else {
        [nswindow toggleFullScreen:nil];
//        [nswindow setCollectionBehavior:NSWindowCollectionBehaviorDefault];
    }
}
