#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QFileDialog>

#import <objc/runtime.h>
#import <Cocoa/Cocoa.h>

#import "mac_utils.h"

static inline NSString* fromQString(const QString &string)
{
    const QByteArray utf8 = string.toUtf8();
    const char* cString = utf8.constData();
    return [[NSString alloc] initWithUTF8String:cString];
}

static inline NSArray *fromQStringList(const QStringList &strings) {
    NSMutableArray *array = [NSMutableArray array];

    foreach (const QString& item, strings) {
        [array addObject:fromQString(item)];
    }

    return array;
}

static inline QString toQString(NSString *string)
{
    if (!string)
        return QString();

    return QString::fromUtf8([string UTF8String]);
}

BOOL isSandboxed() {
    NSDictionary* environ = [[NSProcessInfo processInfo] environment];
    return (nil != [environ objectForKey:@"APP_SANDBOX_CONTAINER_ID"]);
}

void saveFileBookmark(NSString *path) {
    NSLog(@"saveFileBookmark(): %@", path);

    NSURL *url = [NSURL fileURLWithPath:path];

    NSError *error = nil;
    NSData *data = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
    if (error) {
        NSLog(@"Error securing bookmark for url %@ %@", url, error);
    }

    NSLog(@"Data length: %d", data.length);
    NSUserDefaults* prefs = [NSUserDefaults standardUserDefaults];
    NSDictionary *savedEntitlements = [prefs dictionaryForKey:@"directoryEntitlements"];

    NSMutableDictionary *entitlements = [NSMutableDictionary dictionary];
    if (savedEntitlements) {
        NSLog(@"saveFileBookmark(): existing keys %@", entitlements.allKeys);
        [entitlements addEntriesFromDictionary:savedEntitlements];
    }

    [entitlements setObject:data forKey:path];
    [prefs setObject:entitlements forKey:@"directoryEntitlements"];
    [prefs synchronize];
}

void mac_saveFileBookmark(const QString& qpath) {
    if (!isSandboxed())
        return;

    NSString *path = fromQString(qpath);

    saveFileBookmark(path);
}

void mac_restoreFileAccess() {
    if (!isSandboxed())
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
    if (!isSandboxed())
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

QString mac_getExistingDirectory(QWidget *parent,
                                    const QString &caption,
                                    const QString &dir,
                                    QFileDialog::Options options) {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setTitle:fromQString(caption)];
    [panel setCanChooseDirectories:YES];
    [panel setCanCreateDirectories:YES];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseFiles:NO];

    if ([panel runModal] == NSOKButton) {
        if (isSandboxed()) {
            saveFileBookmark(panel.URL.path);
        }

        return toQString(panel.URL.path);
    }

    return QString();
}

QString mac_getOpenFileName(QWidget *parent,
                                 const QString &caption,
                                 const QString &dir,
                                 const QStringList &extensions,
                                 QFileDialog::Options options) {

    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setTitle:fromQString(caption)];
    [panel setCanChooseDirectories:NO];
    [panel setCanCreateDirectories:YES];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseFiles:YES];
    if (extensions.size() != 0)
        [panel setAllowedFileTypes:fromQStringList(extensions)];

    if ([panel runModal] == NSOKButton) {
        if (isSandboxed()) {
            saveFileBookmark(panel.URL.path);
        }

        return toQString(panel.URL.path);
    }

    return QString();
}

QStringList mac_getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir, const QStringList &extensions, QFileDialog::Options options) {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setTitle:fromQString(caption)];
    [panel setCanChooseDirectories:NO];
    [panel setCanCreateDirectories:YES];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseFiles:YES];
    if (extensions.size() != 0)
        [panel setAllowedFileTypes:fromQStringList(extensions)];

    if ([panel runModal] == NSOKButton) {
        QStringList urls;
        for (int i = 0; i < panel.URLs.count; ++i)
            urls.append(toQString([[[panel URLs] objectAtIndex:i] path]));

        if (isSandboxed()) {
            saveFileBookmark(panel.URL.path);
        }

        return urls;
    }

    return QStringList();
}

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
                                           [nswindow setCollectionBehavior:NSWindowCollectionBehaviorDefault];
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
        [nswindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
        [nswindow toggleFullScreen:nil];
    } else {
        [nswindow toggleFullScreen:nil];
    }
}

bool mac_isFullscreen(void *winId) {
    NSView *nsview = (NSView *) winId;
    NSWindow *nswindow = [nsview window];

    bool isFullScreen = [nswindow styleMask] & NSFullScreenWindowMask;
    return isFullScreen;
}
