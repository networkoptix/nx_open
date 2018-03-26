#include "video_data_receptor.h"

#include <nx/kit/debug.h>
#include <nx/sdk/metadata/common_uncompressed_video_frame.h>
#include "nx/utils/log/log_main.h"

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk::metadata;

/*static*/ CommonCompressedVideoPacket* VideoDataReceptor::compressedVideoDataToPlugin(
    const QnConstCompressedVideoDataPtr& frame, bool needDeepCopy)
{
    if (!frame)
        return nullptr;

    nxpt::ScopedRef<CommonCompressedVideoPacket> packet(new CommonCompressedVideoPacket());

    packet->setTimestampUsec(frame->timestamp);
    packet->setWidth(frame->width);
    packet->setHeight(frame->height);
    packet->setCodec(toString(frame->compressionType).toStdString());

    if (needDeepCopy)
    {
        std::vector<char> buffer(frame->dataSize());
        if (!buffer.empty())
            memcpy(&buffer.front(), frame->data(), frame->dataSize());
        packet->setOwnedData(std::move(buffer));
    }
    else
    {
        packet->setExternalData(frame->data(), (int) frame->dataSize());
    }

    return packet.get();
}

/*static*/ UncompressedVideoFrame* VideoDataReceptor::videoDecoderOutputToPlugin(
    const CLConstVideoDecoderOutputPtr& frame, bool needDeepCopy)
{
    // TODO: Consider supporting other decoded video frame formats.
    const auto expectedFormat = AV_PIX_FMT_YUV420P;
    if (frame->format != expectedFormat)
    {
        NX_DEBUG(typeid(VideoDataReceptor)) << lm(
            "ERROR: Decoded frame has AVPixelFormat %1 instead of %2; ignoring")
            .args(frame->format, expectedFormat);
        return nullptr;
    }

    nx::sdk::metadata::UncompressedVideoFrame* uncompressedFrame;
// TODO: #dmishin take needed pixel format from manifest
#if 0
    uncompressedFrame = convertToYuv420pSdkFrame(frame, needDeepCopy);
#else
    uncompressedFrame = convertToArgbSdkFrame(frame, needDeepCopy);
#endif
    return uncompressedFrame;
}

/*static*/ nx::sdk::metadata::UncompressedVideoFrame* VideoDataReceptor::convertToYuv420pSdkFrame(
    CLConstVideoDecoderOutputPtr frame,
    bool needDeepCopy)
{
    const int planeCount = 3;
    const auto packet = new CommonUncompressedVideoFrame(
        frame->pkt_dts, frame->width, frame->height);

    if (needDeepCopy)
    {
        std::vector<std::vector<char>> data(planeCount);
        std::vector<int> lineSize(planeCount);
        for (int i = 0; i < planeCount; ++i)
        {
            const int planeLineSize = frame->linesize[i];
            const int planeDataSize = (i == 0)
                ? (planeLineSize * frame->height) //< Y plane has full resolution.
                : (planeLineSize * frame->height / 2); //< Cb and Cr planes have half resolution.
            data[i].resize(planeDataSize);
            if (planeDataSize > 0)
                memcpy(&data[i].front(), frame->data[i], planeDataSize);
            lineSize[i] = planeLineSize;
        }

        packet->setOwnedData(std::move(data), std::move(lineSize));
    }
    else
    {
        std::vector<const char*> data(planeCount);
        std::vector<int> dataSize(planeCount);
        std::vector<int> lineSize(planeCount);
        for (int i = 0; i < planeCount; ++i)
        {
            const int planeLineSize = frame->linesize[i];
            const int planeDataSize = (i == 0)
                ? (planeLineSize * frame->height) //< Y plane has full resolution.
                : (planeLineSize * frame->height / 2); //< Cb and Cr planes have half resolution.

            data[i] = (const char*)frame->data[i];
            dataSize[i] = planeDataSize;
            lineSize[i] = frame->linesize[i];
        }

        packet->setExternalData(std::move(data), std::move(dataSize), std::move(lineSize));
    }

    return packet;
}

/*static*/ nx::sdk::metadata::UncompressedVideoFrame* VideoDataReceptor::convertToArgbSdkFrame(
    CLConstVideoDecoderOutputPtr frame,
    bool needDeepCopy)
{
    const auto packet = new CommonUncompressedVideoFrame(
        frame->pkt_dts, frame->width, frame->height);
    packet->setPixelFormat(UncompressedVideoFrame::PixelFormat::argb);
    std::vector<std::vector<char>> data(1);
    std::vector<int> lineSize(1);
    data[0] = frame->toArgb(&lineSize[0]);

    packet->setOwnedData(std::move(data), std::move(lineSize));
    return packet;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
