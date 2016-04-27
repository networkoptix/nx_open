#pragma once
#if defined(ENABLE_PROXY_DECODER)

#include <memory>

#include <QtCore/QtGlobal>

#include <proxy_decoder.h>

#include <nx/streaming/video_data_packet.h>

// Uses configuration:
// #define ENABLE_LOG
// #define ENABLE_TIME

void showFps();

/** Debug tool - implicitly unaligned pointer. */
uint8_t* debugUnalignPtr(void* data);

/**
 * @return Null if compressedVideoData is null.
 */
std::unique_ptr<ProxyDecoder::CompressedFrame> createUniqueCompressedFrame(
    const QnConstCompressedVideoDataPtr& compressedVideoData);

// TODO mike: Rewrite macros as in proxy_decoder.

#ifdef ENABLE_LOG
    #define QLOG(...) qDebug().nospace() << __VA_ARGS__
#else // ENABLE_LOG
    #define QLOG(...)
#endif // ENABLE_LOG

#ifdef ENABLE_TIME

    #define TIME_BEGIN(TAG) \
        { std::chrono::milliseconds TIME_t0 = getCurrentTime(); const char* const TIME_tag = TAG;

    #define TIME_END logTime(TIME_t0, TIME_tag); }

    #define TIME_START() \
        QElapsedTimer TIME_timer; \
        TIME_timer.restart(); \
        std::vector<qint64> TIME_list; \
        std::vector<QString> TIME_tags

    #define TIME_PUSH(TAG) timePush((TAG), TIME_timer, TIME_list, TIME_tags)

    #define TIME_FINISH(TAG) timeFinish((TAG), TIME_timer, TIME_list, TIME_tags)

#else // ENABLE_TIME
    #define TIME_BEGIN(TAG)
    #define TIME_END
    #define TIME_START()
    #define TIME_PUSH(TAG)
    #define TIME_FINISH(TAG)
#endif // ENABLE_TIME

class YuvBuffer;
typedef std::shared_ptr<YuvBuffer> YuvBufferPtr;
typedef std::shared_ptr<const YuvBuffer> ConstYuvBufferPtr;

static const int kMediaAlignment = 32;

class YuvBuffer
{
public:
    YuvBuffer(const QSize& frameSize)
    :
        m_frameSize(frameSize),
        m_yBuffer(qMallocAligned(m_frameSize.width() * m_frameSize.height(), kMediaAlignment)),
        m_uBuffer(qMallocAligned( m_frameSize.width() * m_frameSize.height() / 4, kMediaAlignment)),
        m_vBuffer(qMallocAligned(m_frameSize.width() * m_frameSize.height() / 4, kMediaAlignment))
    {
        assert(frameSize.width() % 2 == 0);
        assert(frameSize.height() % 2 == 0);
    }

    ~YuvBuffer()
    {
        qFreeAligned(m_yBuffer);
        qFreeAligned(m_uBuffer);
        qFreeAligned(m_vBuffer);
    }

    QSize frameSize() const
    {
        return m_frameSize;
    }

    int yLineSize() const
    {
        return m_frameSize.width();
    }

    int uVLineSize() const
    {
        return m_frameSize.width() / 2;
    }

    uint8_t* y() const
    {
        return static_cast<uint8_t*>(m_yBuffer);
    }

    uint8_t* u() const
    {
        return static_cast<uint8_t*>(m_uBuffer);
    }

    uint8_t* v() const
    {
        return static_cast<uint8_t*>(m_vBuffer);
    }

private:
    const QSize m_frameSize;

    void* m_yBuffer = nullptr;
    void* m_uBuffer = nullptr;
    void* m_vBuffer = nullptr;
};

#endif // ENABLE_PROXY_DECODER
