// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio.h"

#include <AVFAudio/AVFAudio.h>

#include <QDebug>

namespace nx::audio {

void setupAudio()
{
    static const auto options =
        AVAudioSessionCategoryOptionAllowBluetooth
        | AVAudioSessionCategoryOptionAllowBluetoothA2DP
        | AVAudioSessionCategoryOptionDefaultToSpeaker;

    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    [audioSession
        setCategory: AVAudioSessionCategoryPlayAndRecord
        mode: AVAudioSessionModeDefault
        options: options
        error: nil];

    [audioSession setActive: YES error: nil];
}

}
