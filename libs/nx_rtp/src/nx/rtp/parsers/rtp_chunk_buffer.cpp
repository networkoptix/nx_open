// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rtp_chunk_buffer.h"

#include <cstring>

#include <nx/codec/nal_units.h>

namespace {

static const int kDefaultChunkContainerSize = 1024;

} // namespace

namespace nx::rtp {

RtpChunkBuffer::RtpChunkBuffer()
{
    m_chunks.reserve(kDefaultChunkContainerSize);
}

int RtpChunkBuffer::size() const
{
    return m_videoFrameSize;
}

void RtpChunkBuffer::addChunk(int bufferOffset, uint16_t size, bool nalStart)
{
    m_chunks.emplace_back(bufferOffset, size, nalStart);
    if (nalStart)
        m_videoFrameSize += nx::media::nal::kNalUnitSizeLength;
    m_videoFrameSize += size;
}

void RtpChunkBuffer::clear()
{
    m_videoFrameSize = 0;
    m_chunks.clear();
}

void RtpChunkBuffer::backupCurrentData(const uint8_t* currentBufferBase)
{
    int chunksLength = 0;
    for (const auto& chunk: m_chunks)
        chunksLength += chunk.size;

    // The chunks may already have been backed up, so collect them in the spare buffer and swap it
    // in afterwards instead of writing into the buffer that is possibly being read from.
    m_spareChunksBuffer.resize(chunksLength);
    uint8_t* backupBufRaw = m_spareChunksBuffer.data();

    int offset = 0;
    for (auto& chunk: m_chunks)
    {
        const uint8_t* source = chunk.bufferStart ? chunk.bufferStart : currentBufferBase;
        memcpy(backupBufRaw + offset, source + chunk.bufferOffset, chunk.size);
        chunk.bufferStart = backupBufRaw;
        chunk.bufferOffset = offset;
        offset += chunk.size;
    }
    m_nextFrameChunksBuffer.swap(m_spareChunksBuffer);
}

QnWritableCompressedVideoDataPtr RtpChunkBuffer::buildFrame(
    const uint8_t* rtpBuffer, const uint8_t* header, int headerSize)
{
    QnWritableCompressedVideoDataPtr result = QnWritableCompressedVideoDataPtr(
        new QnWritableCompressedVideoData(m_videoFrameSize + headerSize));

    if (header)
        result->m_data.write((char*) header, headerSize);

    for (size_t i = 0; i < m_chunks.size(); ++i)
    {
        if (m_chunks[i].nalStart)
        {
            // Calculate NAL unit size.
            int nalUnitSize = m_chunks[i].size;
            for (size_t j = i + 1; j < m_chunks.size(); ++j)
            {
                if (m_chunks[j].nalStart)
                    break;

                nalUnitSize += m_chunks[j].size;
            }
            const uint32_t sizeData = htonl(nalUnitSize);
            result->m_data.write((const char*) &sizeData, sizeof(uint32_t));
        }

        const auto chunkBufferStart = m_chunks[i].bufferStart
            ? (const char*) m_chunks[i].bufferStart
            : (const char*) rtpBuffer;

        result->m_data.write(chunkBufferStart + m_chunks[i].bufferOffset, m_chunks[i].size);
    }
    return result;
}

} // namespace nx::rtp
