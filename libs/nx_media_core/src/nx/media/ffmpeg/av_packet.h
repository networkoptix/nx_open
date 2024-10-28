// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

struct AVPacket;
struct QnAbstractMediaData;

namespace nx::media::ffmpeg {

class NX_MEDIA_CORE_API AvPacket
{
public:
    AvPacket(uint8_t* data = nullptr, int size = 0);
    AvPacket(const QnAbstractMediaData* data);
    ~AvPacket();
    AVPacket* get() { return m_packet; }

    AvPacket(const AvPacket&) = delete;
    AvPacket& operator=(const AvPacket&) = delete;

private:
    AVPacket* m_packet = nullptr;
};

} // namespace nx::media::ffmpeg
