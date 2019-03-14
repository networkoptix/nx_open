#include "utils.h"
#include <array>

#include <utils/media/jpeg_utils.h>
#include <utils/media/h263_utils.h>
#include <utils/media/h264_utils.h>
#include <utils/media/hevc_sps.h>

namespace nx {
namespace media {

QSize getFrameSize(const QnConstCompressedVideoDataPtr& frame)
{
    if (frame->width > 0 && frame->height > 0)
        return QSize(frame->width, frame->height);

    switch (frame->compressionType)
    {
        case AV_CODEC_ID_H265:
        {
            nx::media_utils::hevc::Sps sps;
            if (sps.decodeFromVideoFrame(frame))
                return QSize(sps.picWidthInLumaSamples, sps.picHeightInLumaSamples);
            return QSize();
        }
        case AV_CODEC_ID_H264:
        {
            SPSUnit sps;
            if (nx::media_utils::h264::extractSps(frame, sps))
                return QSize(sps.getWidth(), sps.getHeight());
            return QSize();
        }
        case AV_CODEC_ID_MJPEG:
        {
            nx_jpg::ImageInfo imgInfo;
            nx_jpg::readJpegImageInfo((const quint8*) frame->data(), frame->dataSize(), &imgInfo);
            return QSize(imgInfo.width, imgInfo.height);
        }
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
        {
            nx::media_utils::h263::PictureHeader header;
            if (header.decode((uint8_t*)frame->data(), frame->dataSize()))
                return QSize(header.width, header.height);

            return QSize();
        }
        default:
            return QSize();
    }
}

double getDefaultSampleAspectRatio(const QSize& srcSize)
{
    struct AspectRatioData
    {
        AspectRatioData() {}
        AspectRatioData(const QSize& srcSize, const QSize& dstAspect) :
            srcSize(srcSize),
            dstAspect(dstAspect)
        {
        }

        QSize srcSize;
        QSize dstAspect;
    };

    static std::array<AspectRatioData, 6> kOverridedAr =
    {
        AspectRatioData(QSize(720, 576), QSize(4, 3)),
        AspectRatioData(QSize(720, 480), QSize(4, 3)),
        AspectRatioData(QSize(720, 240), QSize(4, 3)),
        AspectRatioData(QSize(704, 576), QSize(4, 3)),
        AspectRatioData(QSize(704, 480), QSize(4, 3)),
        AspectRatioData(QSize(704, 240), QSize(4, 3))
    };

    for (const auto& data: kOverridedAr)
    {
        if (data.srcSize == srcSize)
        {
            double dstAspect = (double) data.dstAspect.width() / data.dstAspect.height();
            double srcAspect = (double) srcSize.width() / srcSize.height();
            return dstAspect / srcAspect;
        }
    }

    return 1.0;
}

} // namespace media
} // namespace nx
