// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/ffmpeg/frame_info.h>

namespace nx::streaming {

class NX_VMS_COMMON_API UncompressedVideoPacket: public QnAbstractDataPacket
{
public:
    UncompressedVideoPacket(CLConstVideoDecoderOutputPtr uncompressedFrame);

    virtual CLConstVideoDecoderOutputPtr frame() const;

private:
    CLConstVideoDecoderOutputPtr m_frame;
};

} // namespace nx::streaming
