// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <cstdint>

#include <nx/media/video_data_packet.h>

namespace nx::rtp {

class RtpChunkBuffer
{
public:
    RtpChunkBuffer();
    void addChunk(int bufferOffset, uint16_t len, bool nalStart = false);
    QnWritableCompressedVideoDataPtr buildFrame(
        const uint8_t* rtpBuffer, const uint8_t* header, int headerSize);
    void backupCurrentData(const uint8_t* currentBufferBase);
    void clear();
    int size() const;

private:
    struct Chunk
    {
        Chunk() = default;
        Chunk(int bufferOffset, uint16_t size, bool nalStart = false):
            bufferStart(nullptr),
            bufferOffset(bufferOffset),
            size(size),
            nalStart(nalStart)
        {
        }

        uint8_t* bufferStart = nullptr;
        int bufferOffset = 0;
        uint16_t size = 0;
        bool nalStart = false;
    };

private:
    int m_videoFrameSize = 0;
    std::vector<Chunk> m_chunks;
    std::vector<uint8_t> m_nextFrameChunksBuffer;
};

} // namespace nx::rtp
