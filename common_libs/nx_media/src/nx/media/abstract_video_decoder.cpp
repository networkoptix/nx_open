#include "abstract_video_decoder.h"

#include <utils/media/jpeg_utils.h>
#include <utils/media/h264_utils.h>
#include <utils/media/hevc_sps.h>

namespace nx {
namespace media {

QSize AbstractVideoDecoder::mediaSizeFromRawData(const QnConstCompressedVideoDataPtr& frame)
{
    switch (frame->context->getCodecId())
    {
        case AV_CODEC_ID_H265:
        {
            QSize result;
            nx::media_utils::hevc::Sps sps;
            if (sps.decodeFromVideoFrame(frame))
                result = QSize(sps.picWidthInLumaSamples, sps.picHeightInLumaSamples);
            return result;
        }
        case AV_CODEC_ID_H264:
        {
            QSize result;
            nx::media_utils::avc::extractSpsPps(frame, &result, nullptr);
            return result;
        }
        case AV_CODEC_ID_MJPEG:
        {
            nx_jpg::ImageInfo imgInfo;
            nx_jpg::readJpegImageInfo((const quint8*) frame->data(), frame->dataSize(), &imgInfo);
            return QSize(imgInfo.width, imgInfo.height);
        }
        default:
            return QSize();
    }
}

} // namespace media
} // namespace nx
