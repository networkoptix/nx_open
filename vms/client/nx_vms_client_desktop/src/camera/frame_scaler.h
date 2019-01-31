#ifndef QN_FRAME_SCALER_H
#define QN_FRAME_SCALER_H

#include "utils/media/frame_info.h"

class QnFrameScaler
{
public:
    // TODO: #Elric #enum
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

    static void downscale(const CLVideoDecoderOutput* src, CLVideoDecoderOutput* dst, DownscaleFactor factor);

private:
    static void downscalePlate_factor2(unsigned char* dst, int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor4(unsigned char* dst, int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height);
    static void downscalePlate_factor8(unsigned char* dst, int dstStride, const unsigned char* src, int src_width, int src_stride, int src_height);
};

#endif // QN_FRAME_SCALER_H
