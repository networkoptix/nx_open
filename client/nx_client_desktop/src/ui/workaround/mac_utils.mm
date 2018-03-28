#include <QtCore/QAbstractEventDispatcher>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QtMath>
#include <QtCore/QEventLoop>

#include <sys/resource.h>

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

NSArray* fromQStringList(const QStringList& strings)
{
    auto array = [NSMutableArray array];
    for (const auto& item: strings)
        [array addObject:fromQString(item)];

    return array;
}

QString toQString(NSString* string)
{
    return string
        ? QString::fromUtf8([string UTF8String])
        : QString();
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

enum class OpenDialogMode
{
    chooseFile,
    chooseMultipleFiles,
    chooseDirectory
};

QStringList showOpenDialogInternal(
    OpenDialogMode mode,
    const QString& caption,
    const QString& directory,
    const QStringList& extensions = QStringList())
{
    const bool sandboxed = mac_isSandboxed();
    const auto panel = [NSOpenPanel openPanel];
    [panel setTitle:fromQString(caption)];
    [panel setCanCreateDirectories:YES];
    [panel setAllowsMultipleSelection: mode == OpenDialogMode::chooseMultipleFiles ? YES : NO];
    [panel setCanChooseFiles: mode == OpenDialogMode::chooseDirectory ? NO : YES];
    [panel setCanChooseDirectories: mode == OpenDialogMode::chooseDirectory ? YES : NO];

    if (extensions.size() != 0)
        [panel setAllowedFileTypes:fromQStringList(extensions)];

    if (!sandboxed && !directory.isEmpty())
        [panel setDirectoryURL:[NSURL fileURLWithPath:fromQString(directory)]];

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
    const bool ok = ([panel runModal] == NSModalResponseOK);
    QAbstractEventDispatcher::instance()->interrupt();

    if (!ok)
        return QStringList();

    QStringList result;
    if (mode == OpenDialogMode::chooseMultipleFiles)
    {
        for (size_t i = 0; i < panel.URLs.count; ++i)
            result.append(toQString([[[panel URLs] objectAtIndex:i] path]));
    }
    else
    {
        result.append(toQString(panel.URL.path));
    }

    if (sandboxed)
        saveFileBookmark(panel.URL.path);

    return result;
}

template<typename ResultType>
class DelayedCall: public QObject
{
    using base_type = QObject;

public:
    using FunctionType = std::function<ResultType ()>;

    static ResultType execute(FunctionType function);

private:
    DelayedCall(QEventLoop& loop, FunctionType function);

    ResultType result() const;

    virtual bool event(QEvent* e) override;

private:
    QEventLoop& m_loop;
    const FunctionType m_function;
    ResultType m_result;
};

template<typename ResultType>
DelayedCall<ResultType>::DelayedCall(QEventLoop& loop, FunctionType function):
    m_loop(loop),
    m_function(function)
{
}

template<typename ResultType>
bool DelayedCall<ResultType>::event(QEvent* e)
{
    if (e->type() != QEvent::User)
        return base_type::event(e);

    if (m_function)
        m_result = m_function();
    m_loop.quit();
    return true;
}

template<typename ResultType>
ResultType DelayedCall<ResultType>::execute(FunctionType function)
{
    QEventLoop loop;
    DelayedCall<ResultType> delayedCall(loop, function);
    qApp->postEvent(&delayedCall, new QEvent(QEvent::User));
    loop.exec();
    return delayedCall.result();
}

template<typename ResultType>
ResultType DelayedCall<ResultType>::result() const
{
    return m_result;
}

// We have to use our oun event loop to prevent dialog from sudden hide. Otherwise, it looks like
// somewone sends message to close dialog unexpectecly.
// We can't use executeDelayed function since sometimes event loop is broken (Qt bug) and it does
// not fire. So, we have to use qApp->postEvent.
QStringList showOpenDialog(
    OpenDialogMode mode,
    const QString& caption,
    const QString& directory,
    const QStringList& extensions = QStringList())
{
    return DelayedCall<QStringList>::execute(
        [mode, caption, directory, extensions]()
        {
            return showOpenDialogInternal(mode, caption, directory, extensions);
        });
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

QString mac_getExistingDirectory(
    const QString& caption,
    const QString& directory)
{
    const auto result = showOpenDialog(
        OpenDialogMode::chooseDirectory, caption, directory);
    return result.isEmpty() ? QString() : result.front();
}

QString mac_getOpenFileName(
    const QString& caption,
    const QString& directory,
    const QStringList& extensions)
{
    const auto result = showOpenDialog(
        OpenDialogMode::chooseFile, caption, directory, extensions);
    return result.isEmpty() ? QString() : result.front();
}

QStringList mac_getOpenFileNames(
    const QString& caption,
    const QString& directory,
    const QStringList& extensions)
{
    return showOpenDialog(
        OpenDialogMode::chooseMultipleFiles, caption, directory, extensions);
}

// We have to use our oun event loop to prevent dialog from sudden hide. Otherwise, it looks like
// somewone sends message to close dialog unexpectecly.
// We can't use executeDelayed function since sometimes event loop is broken (Qt bug) and it does
// not fire. So, we have to use qApp->postEvent.

QString mac_getSaveFileName(
    const QString& caption,
    const QString& directory,
    const QStringList& extensions)
{
    return DelayedCall<QString>::execute(
        [caption, directory, extensions]()
        {
            const bool sandboxed = mac_isSandboxed();

            const auto panel = [NSSavePanel savePanel];
            [panel setTitle:fromQString(caption)];
            [panel setCanCreateDirectories:YES];
            if (extensions.size() != 0)
                [panel setAllowedFileTypes:fromQStringList(extensions)];
            [panel setAllowsOtherFileTypes:YES];
            if (!sandboxed && !directory.isEmpty())
                [panel setDirectoryURL:[NSURL fileURLWithPath:fromQString(directory)]];

            qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
            const bool ok = ([panel runModal] == NSModalResponseOK);
            QAbstractEventDispatcher::instance()->interrupt();

            if (!ok)
                return QString();

            if (sandboxed)
            {
                // This is a hack! We cannot save bookmark for an inexistent file.
                // So we create it before bookmark saving.
                QFile file(toQString(panel.URL.path));
                file.open(QFile::WriteOnly);
                file.close();

                saveFileBookmark(panel.URL.path);

                // ... and remove after
                file.remove();
            }

            return toQString(panel.URL.path);
        });
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
        mac_disableFullscreenButton(winId);
    }
}

bool mac_isFullscreen(void *winId) {
    NSView *nsview = (NSView *) winId;
    NSWindow *nswindow = [nsview window];

    bool isFullScreen = [nswindow styleMask] & NSFullScreenWindowMask;
    return isFullScreen;
}

void mac_setLimits() {
    /* In MacOS the default limit to maximum number of file descriptors is 256.
     * It is NOT enough for our program. Manual tests showed that 4096 was enough
     * for the scene full of identical elements.
     * But let it be twice more, just to be sure... */
    const rlim_t wantedLimit = 8192;

    struct rlimit limit;
    getrlimit(RLIMIT_NOFILE, &limit);

    if (limit.rlim_cur < wantedLimit)
        limit.rlim_cur = qMin(wantedLimit, limit.rlim_max);
    setrlimit(RLIMIT_NOFILE, &limit);
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
