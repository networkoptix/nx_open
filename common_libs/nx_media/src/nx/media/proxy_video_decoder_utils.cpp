#include "proxy_video_decoder_utils.h"
#if defined(ENABLE_PROXY_DECODER)

#include <chrono>
#include <deque>
#include <vector>

#include <QtCore/QString>
#include <QtCore/QElapsedTimer>

std::chrono::milliseconds getCurrentTime()
{
    Q_UNUSED(getCurrentTime);

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

void logTime(std::chrono::milliseconds oldTime, const char* tag)
{
    std::chrono::milliseconds time = getCurrentTime();
    qWarning() << "TIME ms:" << (time - oldTime).count() << " [" << tag << "]";
}

void timePush(
    const char* tag, const QElapsedTimer& timer,
    std::vector<qint64>& list, std::vector<QString>& tags)
{
    const auto t = timer.elapsed();
    if (std::find(tags.begin(), tags.end(), QString(tag)) != tags.end())
    {
        std::cerr << "FATAL INTERNAL ERROR: tag \"" << tag << "\" already pushed to time list.\n";
        NX_CRITICAL(false);
    }
    list.push_back(t);
    tags.push_back(tag);
}

void timeFinish(
    const char* tag, const QElapsedTimer& timer,
    std::vector<qint64>& list, std::vector<QString>& tags)
{
    auto now = timer.elapsed();
    QString s;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (i > 0)
            s.append(", ");
        s.append(tags.at(i) + QString(": ") + QString::number(list.at(i)));
    }
    if (list.size() > 0)
        s.append(", ");
    s.append("last: ");
    s.append(QString::number(now));
    qWarning().nospace() << "TIME [" << tag << "]: " << qPrintable(s);
}

void showFps()
{
    static const int kFpsCount = 30;
    static std::deque<int> deltaList;
    static std::chrono::milliseconds prevT(0);

    const std::chrono::milliseconds t = getCurrentTime();
    if (prevT.count() != 0)
    {
        const int delta = (t - prevT).count();
        deltaList.push_back(delta);
        if (deltaList.size() > kFpsCount)
            deltaList.pop_front();
        assert(!deltaList.empty());
        double deltaAvg = std::accumulate(deltaList.begin(), deltaList.end(), 0.0)
            / deltaList.size();

        qWarning().nospace() << "FPS: Avg: "
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

#endif // ENABLE_PROXY_DECODER
