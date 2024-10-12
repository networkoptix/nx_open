// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/ffmpeg/frame_info.h>

namespace nx::media::nvidia {

class NvidiaVideoDecoder;

class NX_MEDIA_API NvidiaVideoFrame: public AbstractVideoSurface
{
public:
    ~NvidiaVideoFrame();
    virtual AVFrame lockFrame() override;
    virtual void unlockFrame() override;
    virtual SurfaceType type() override { return SurfaceType::Nvidia; }

public:
    int width = 0;
    int height = 0;
    int format = 0;
    int pitch = 0;
    int bitDepth = 0;
    int matrix = 0;
    int64_t timestamp = 0;
    uint8_t* frameData = nullptr;
    int bufferSize = 0;
    std::weak_ptr<nx::media::nvidia::NvidiaVideoDecoder> decoder;

private:
    std::vector<uint8_t> systemMemory;
};

} // namespace nx::media::nvidia
