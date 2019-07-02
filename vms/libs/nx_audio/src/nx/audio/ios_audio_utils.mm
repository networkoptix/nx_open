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
