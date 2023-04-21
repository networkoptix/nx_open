// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio.h"

#include <QtCore/qsystemdetection.h>

#if !defined(Q_OS_IOS)

namespace nx::audio {

void setupAudio()
{
}

} //namespace nx::audio

#endif
