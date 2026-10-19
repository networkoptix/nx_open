// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_bundle_launcher.h"

#import <AppKit/AppKit.h>

#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <spawn.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <nx/branding.h>
#include <nx/vms/applauncher/api/applauncher_api.h>

namespace {

// Applaunchers up to 4.2 start the binary directly without "--run", forwarding again would loop.
constexpr const char* kForwardedLaunchVariable = "NX_CLIENT_LAUNCH_FORWARDED";

// Launch-time open requests are delivered before applicationDidFinishLaunching, the delay covers
// the late ones only.
constexpr NSTimeInterval kOpenRequestsGracePeriodS = 0.1;

NSString* applauncherScriptPath()
{
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string executablePath(size, '\0');
    if (_NSGetExecutablePath(executablePath.data(), &size) != 0)
        return nil;

    NSString* directory = [@(executablePath.c_str()) stringByDeletingLastPathComponent];
    NSString* path = [directory stringByAppendingPathComponent:
        nx::branding::minilauncherBinaryName().toNSString()];

    return [[NSFileManager defaultManager] isExecutableFileAtPath:path] ? path : nil;
}

void startApplauncher(NSString* scriptPath, NSArray<NSString*>* arguments)
{
    std::vector<std::string> storage{scriptPath.fileSystemRepresentation};
    for (NSString* argument in arguments)
        storage.push_back(argument.fileSystemRepresentation);

    std::vector<char*> argv;
    for (auto& value: storage)
        argv.push_back(value.data());
    argv.push_back(nullptr);

    setenv(kForwardedLaunchVariable, "1", /*overwrite*/ 1);

    pid_t pid = 0;
    const int error =
        posix_spawn(&pid, argv[0], nullptr, nullptr, argv.data(), *_NSGetEnviron());
    if (error != 0)
        NSLog(@"Failed to start %@: %s", scriptPath, strerror(error));
}

} // namespace

@interface NxBundleLaunchForwarder: NSObject<NSApplicationDelegate>
{
    NSString* m_scriptPath;
    NSMutableArray<NSString*>* m_arguments;
}

- (instancetype)initWithScriptPath:(NSString*)scriptPath arguments:(NSArray<NSString*>*)arguments;

@end

@implementation NxBundleLaunchForwarder

- (instancetype)initWithScriptPath:(NSString*)scriptPath arguments:(NSArray<NSString*>*)arguments
{
    if (self = [super init])
    {
        m_scriptPath = [scriptPath copy];
        m_arguments = [arguments mutableCopy];
    }
    return self;
}

- (void)dealloc
{
    [m_scriptPath release];
    [m_arguments release];
    [super dealloc];
}

// Receives both URL scheme and file open requests.
- (void)application:(NSApplication*)application openURLs:(NSArray<NSURL*>*)urls
{
    for (NSURL* url in urls)
        [m_arguments addObject:(url.isFileURL ? url.path : url.absoluteString)];
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    [self performSelector:@selector(forward) withObject:nil afterDelay:kOpenRequestsGracePeriodS];
}

- (void)forward
{
    startApplauncher(m_scriptPath, m_arguments);

    // -[NSApplication stop:] takes effect only after the next event is processed.
    [NSApp stop:nil];
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0]
             atStart:NO];
}

@end

namespace nx::vms::client::desktop {

bool forwardMacBundleLaunch(int* argc, char** argv)
{
    using nx::vms::applauncher::api::kMacRunClientOption;

    char** const end = argv + *argc;
    if (char** const option = std::find_if(argv + 1, end,
            [](const char* arg) { return std::strcmp(arg, kMacRunClientOption) == 0; });
        option != end)
    {
        std::move(option + 1, end + 1, option); //< Including the terminating nullptr.
        --*argc;
        return false;
    }

    if (getenv(kForwardedLaunchVariable))
        return false;

    @autoreleasepool
    {
        NSString* scriptPath = applauncherScriptPath();
        if (!scriptPath)
            return false;

        NSMutableArray<NSString*>* arguments = [NSMutableArray array];
        for (int i = 1; i < *argc; ++i)
        {
            [arguments addObject:[[NSFileManager defaultManager]
                stringWithFileSystemRepresentation:argv[i] length:strlen(argv[i])]];
        }

        NSApplication* application = [NSApplication sharedApplication];
        [application setActivationPolicy:NSApplicationActivationPolicyAccessory];
        NxBundleLaunchForwarder* forwarder =
            [[[NxBundleLaunchForwarder alloc] initWithScriptPath:scriptPath arguments:arguments]
                autorelease];
        application.delegate = forwarder;
        [application run];
    }
    return true;
}

} // namespace nx::vms::client::desktop
