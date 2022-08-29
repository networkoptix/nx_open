// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/io_context.h>

namespace nx::media::ffmpeg {

class Demuxer
{
public:
    ~Demuxer();
    bool open(IoContextPtr ioContext);
    QnAbstractMediaDataPtr getNextData();
    void close();

private:
    IoContextPtr m_ioContext;
    AVFormatContext* m_formatContext = nullptr;
};

} // namespace nx::media::ffmpeg