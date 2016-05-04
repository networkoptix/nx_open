#include "proxy_video_decoder_utils.h"
#if defined(ENABLE_PROXY_DECODER)

#include <cstdint>
#include <deque>

namespace nx {
namespace media {

ProxyVideoDecoderFlagConfig conf("ProxyVideoDecoder");

long getTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void logTimeMs(long oldTimeMs, const char* tag)
{
    long timeMs = getTimeMs();
    PRINT << "TIME ms: " << (timeMs - oldTimeMs) << " [" << tag << "]";
}

struct DebugTimer::Private
{
    const char* name;
    QElapsedTimer timer;
    std::vector<qint64> list;
    std::vector<QString> tags;
};

DebugTimer::DebugTimer(const char* name)
{
    if (conf.enableTime)
    {
        d.reset(new Private);
        d->name = name;
        d->timer.restart();
    }
}

DebugTimer::~DebugTimer() //< Destructor should be defined to satisfy unique_ptr.
{
}

void DebugTimer::mark(const char* tag)
{
    if (d)
    {
        const auto t = d->timer.elapsed();
        if (std::find(d->tags.begin(), d->tags.end(), QString(tag)) != d->tags.end())
            NX_CRITICAL(false, lm("INTERNAL ERROR: Timer mark '%s' already defined").arg(tag));
        d->list.push_back(t);
        d->tags.push_back(tag);
    }
}

void DebugTimer::finish(const char* tag)
{
    if (d)
    {
        mark(tag);

        auto now = d->timer.elapsed();
        QString s;
        for (int i = 0; i < (int) d->list.size(); ++i)
        {
            if (i > 0)
                s.append(", ");
            s.append(d->tags.at(i) + QString(": ") + QString::number(d->list.at(i)));
        }
        if (d->list.size() > 0)
            s.append(", ");
        s.append("last: ");
        s.append(QString::number(now));
        PRINT << "TIME [" << d->name << "]: " << qPrintable(s);
    }
}

void debugShowFps()
{
    static const int kFpsCount = 30;
    static std::deque<long> deltaList;
    static long prevT = 0;

    const long t = getTimeMs();
    if (prevT != 0)
    {
        const long delta = t - prevT;
        deltaList.push_back(delta);
        if (deltaList.size() > kFpsCount)
            deltaList.pop_front();
        assert(!deltaList.empty());
        double deltaAvg = std::accumulate(deltaList.begin(), deltaList.end(), 0.0)
            / deltaList.size();

        PRINT << "FPS: Avg: "
            << qPrintable(QString::number(1000.0 / deltaAvg, 'f', 1))
            << ", ms: " << delta << ", avg ms: " << deltaAvg;
    }
    prevT = t;
}

uint8_t* debugUnalignPtr(void* data)
{
    return (uint8_t*) (
        (17 + ((uintptr_t) data) + kMediaAlignment - 1) / kMediaAlignment * kMediaAlignment);
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

    uint32_t* pLine = ((uint32_t*) argbBuffer) + line0 * lineLen;
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
        compressedFrame->data = (const uint8_t*) compressedVideoData->data();
        compressedFrame->dataSize = (int) compressedVideoData->dataSize();
        compressedFrame->pts = compressedVideoData->timestamp;
        compressedFrame->isKeyFrame =
            (compressedVideoData->flags & QnAbstractMediaData::MediaFlags_AVKey) != 0;
    }

    return compressedFrame;
}

} // namespace media
} // namespace nx

#endif // ENABLE_PROXY_DECODER
