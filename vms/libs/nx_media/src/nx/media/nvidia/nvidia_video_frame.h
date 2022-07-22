// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utils/media/frame_info.h>

namespace nx::media::nvidia {
class NvidiaVideoDecoder;
}

class NvidiaVideoFrame: public AbstractVideoSurface
{
public:
    virtual AVFrame lockFrame() override;
    virtual void unlockFrame() override;

public:
    int width = 0;
    int height = 0;
    int format = 0;
    int pitch = 0;
    int bitDepth = 0;
    int matrix = 0;
    int64_t timestamp = 0;
    uint8_t* frameData = nullptr;
    std::weak_ptr<nx::media::nvidia::NvidiaVideoDecoder> decoder;
};
