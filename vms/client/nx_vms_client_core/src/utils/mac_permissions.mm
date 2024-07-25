// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_permissions.h"

#include <AVFoundation/AVAudioSession.h>

bool mac_ensureAudioRecordPermission()
{
    return [[AVAudioSession sharedInstance] recordPermission]
        != AVAudioSessionRecordPermissionDenied;
}
