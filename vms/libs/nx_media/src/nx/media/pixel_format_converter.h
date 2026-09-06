// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

struct AVFrame;
struct SwsContext;

namespace nx::media {

/**
 * Converts a decoded AVFrame whose pixel format Qt cannot display directly (a format not handled
 * by AvFrameMemoryBuffer::toQtPixelFormat) into YUV420P using libswscale. Owns the scaling
 * context, created lazily on first use and reused across subsequent frames.
 */
class PixelFormatConverter
{
public:
    PixelFormatConverter() = default;
    ~PixelFormatConverter();

    PixelFormatConverter(const PixelFormatConverter&) = delete;
    PixelFormatConverter& operator=(const PixelFormatConverter&) = delete;

    /**
     * Convert srcFrame to YUV420P. Returns a newly allocated frame owned by the caller (release
     * with av_frame_free), or nullptr on failure.
     */
    AVFrame* toYuv420p(const AVFrame* srcFrame);

private:
    SwsContext* m_scaleContext = nullptr;
};

} // namespace nx::media
