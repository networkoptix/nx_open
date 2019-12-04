#pragma once

#include <utils/media/frame_info.h>

namespace nx::streaming {

class UncompressedVideoPacket: public QnAbstractDataPacket
{
public:
    UncompressedVideoPacket(CLConstVideoDecoderOutputPtr uncompressedFrame);

    virtual CLConstVideoDecoderOutputPtr frame() const;

private:
    CLConstVideoDecoderOutputPtr m_frame;
};

} // namespace nx::streaming
