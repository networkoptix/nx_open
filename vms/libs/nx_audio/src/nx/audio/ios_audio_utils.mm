// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ios_audio_utils.h"

#include <AVFoundation/AVFoundation.h>

namespace nx::audio {
    
void forceSpeakersUsage()
{
    const auto audioSession = [AVAudioSession sharedInstance];
    [audioSession setCategory:AVAudioSessionCategoryPlayAndRecord
        withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker error:nil];
}

} // namespace nx::audio
