// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/audio_data_packet.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/ffmpeg/io_context.h>
#include <nx/media/video_data_packet.h>

namespace nx::media::ffmpeg {

class NX_MEDIA_CORE_API Demuxer
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
