#include "proxy_video_decoder_utils.h"
#if defined(ENABLE_PROXY_DECODER)

#include <cstdint>
#include <deque>

#include <nx/utils/string.h>

namespace nx {
namespace media {

std::ostream& operator<<(std::ostream& stream, const QSize& qSize)
{
    return stream << "(" << qSize.width() << ", " << qSize.height() << ")";
}

std::ostream& operator<<(std::ostream& stream, const QString& qString)
{
    return stream << qString.toUtf8().constData();
}

void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameWidth, int frameHeight)
{
    static const int kBoardSize = 128;
    static const int kShift = 4; //< log2(kBoardSize / 8)

    static int x0 = 0;
    static int line0 = 0;

    assert((lineSizeBytes & 0x03) == 0);
    const int lineLen = lineSizeBytes >> 2;

    if (!(frameWidth >= kBoardSize && frameHeight >= kBoardSize)) //< Frame is too small.
        return;

    uint32_t* pLine = ((uint32_t*)argbBuffer) + line0 * lineLen;
    for (int line = 0; line < kBoardSize; ++line)
    {
        for (int x = 0; x < kBoardSize; ++x)
            pLine[x0 + x] = (((x >> kShift) & 1) == ((line >> kShift) & 1)) ? 0x006480FE : 0;
        pLine += lineLen;
    }

    if (++x0 >= frameWidth - kBoardSize)
        x0 = 0;
    if (++line0 >= frameHeight - kBoardSize)
        line0 = 0;
}

std::unique_ptr<ProxyDecoder::CompressedFrame> createUniqueCompressedFrame(
    const QnConstCompressedVideoDataPtr& compressedVideoData)
{
    std::unique_ptr<ProxyDecoder::CompressedFrame> compressedFrame;
    if (compressedVideoData)
    {
        NX_CRITICAL(compressedVideoData->data());
        NX_CRITICAL(compressedVideoData->dataSize() > 0);
        compressedFrame.reset(new ProxyDecoder::CompressedFrame);
        compressedFrame->data = (const uint8_t*)compressedVideoData->data();
        compressedFrame->dataSize = (int)compressedVideoData->dataSize();
        compressedFrame->ptsUs = compressedVideoData->timestamp;
        compressedFrame->isKeyFrame =
            (compressedVideoData->flags & QnAbstractMediaData::MediaFlags_AVKey) != 0;
    }

    return compressedFrame;
}

} // namespace media
} // namespace nx

#endif // defined(ENABLE_PROXY_DECODER)
