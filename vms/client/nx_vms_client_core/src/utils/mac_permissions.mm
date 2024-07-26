// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_permissions.h"

#include <future>

#include <AVFoundation/AVAudioSession.h>

bool mac_ensureAudioRecordPermission()
{
    // TODO: Switch to AVAudioApplication.requestRecordPermission for iOS 17.0+ and macOS 14.0+.
    switch ([[AVAudioSession sharedInstance] recordPermission])
    {
        case AVAudioSessionRecordPermissionGranted:
            return true;
        case AVAudioSessionRecordPermissionDenied:
            return false;
        case AVAudioSessionRecordPermissionUndetermined:
        {
            __block std::promise<bool> promise;
            std::future<bool> future = promise.get_future();
            [[AVAudioSession sharedInstance] requestRecordPermission:
                ^(BOOL granted)
                {
                    promise.set_value(granted);
                }];

            return future.get();
        }
        default:
            return true; //< Let Qt handle the permission request.
    }
}
