// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QAbstractVideoBuffer>

extern "C" {
#include <libavformat/avformat.h>
} // extern "C"

#include <nx/media/ffmpeg/frame_info.h>

namespace nx::media {


/**
 * This class implements QT video buffer interface using ffmpeg av_frame allocation.
 */
class AvFrameMemoryBuffer: public QAbstractVideoBuffer
{
public:
    AvFrameMemoryBuffer(AVFrame* frame, bool ownsFrame);

    virtual ~AvFrameMemoryBuffer();

    virtual MapData map(QVideoFrame::MapMode mode) override;

    virtual void unmap() override {}

    virtual QVideoFrameFormat format() const;

    static QVideoFrameFormat::PixelFormat toQtPixelFormat(AVPixelFormat pixFormat);

private:
    AVFrame* m_frame = nullptr;
    bool m_ownsFrame = true;
};

class CLVideoDecoderOutputMemBuffer: public AvFrameMemoryBuffer
{
public:
    CLVideoDecoderOutputMemBuffer(const CLVideoDecoderOutputPtr& frame);
private:
    CLVideoDecoderOutputPtr m_frame;
};


} // namespace nx::media
