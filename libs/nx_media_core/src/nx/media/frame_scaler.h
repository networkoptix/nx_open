// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/ffmpeg/frame_info.h>

/*
 * This class is alternative to sws_scale. It is limited to scale frames on 2,4 or 8 times without
 * changing pixel format. It works several times faster than ffmpeg sws_scale.
 */
class NX_MEDIA_CORE_API QnFrameScaler
{
public:
    enum DownscaleFactor {
        factor_any = 0,
        factor_1 = 1,
        factor_2 = 2,
        factor_4 = 4,
        factor_8 = 8,
        factor_16 = 16,
        factor_32 = 32,
        factor_64 = 64,
        factor_128 = 128,
        factor_256 = 256,
        factor_unknown = 512
    };

    static void downscale(const AVFrame* src, CLVideoDecoderOutput* dst, DownscaleFactor factor);

    /*
     * Downscale avFrame if scaler supports it. If function returns false sws_scale should be
     * used as fallback.
     */
    static bool downscale(const AVFrame* src, CLVideoDecoderOutput* dst, bool useIntrinsicsOnly = true);

private:
    static void downscalePlate_factor2(
        unsigned char* dst,
        int dstStride,
        const unsigned char* src,
        int src_width,
        int src_stride,
        int src_height,
        quint8 filler);
    static void downscalePlate_factor4(
        unsigned char* dst,
        int dstStride,
        const unsigned char* src,
        int src_width,
        int src_stride,
        int src_height,
        quint8 filler);
    static void downscalePlate_factor8(
        unsigned char* dst,
        int dstStride,
        const unsigned char* src,
        int src_width,
        int src_stride,
        int src_height,
        quint8 filler);
};
