#pragma once

//-------------------------------------------------------------------------------------------------
// Public

// Uses configuration:
// #define ENABLE_LOG
// #define ENABLE_TIME

// Debug utils.
// #define LL ...
// #define QLOG(...) ...
// #define TIME_BEGIN(TAG) ...
// #define TIME_END ...
// #define TIME_START() ...
// #define TIME_PUSH(TAG) ...
// #define TIME_FINISH(TAG) ...

static void showFps();

static uint8_t* alignPtr(void* data);

/** Debug tool - implicitly unaligned pointer. */
static uint8_t* unalignPtr(void* data);

class YuvBuffer;
typedef std::shared_ptr<YuvBuffer> YuvBufferPtr;
typedef std::shared_ptr<const YuvBuffer> ConstYuvBufferPtr;

//-------------------------------------------------------------------------------------------------

#include <chrono>
#include <deque>

// Log execution of a line - put double-L at the beginning of the line.
#define LL qDebug() << "####### line " << __LINE__;

#ifdef ENABLE_LOG
    #define QLOG(...) qDebug() << __VA_ARGS__
#else // ENABLE_LOG
    #define QLOG(...)
#endif // ENABLE_LOG

std::chrono::milliseconds getCurrentTime()
{
    Q_UNUSED(getCurrentTime);

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

#ifdef ENABLE_TIME

    void logTime(std::chrono::milliseconds oldTime, const char* tag)
    {
        std::chrono::milliseconds time = getCurrentTime();
        qWarning() << "TIME ms:" << (time - oldTime).count() << " [" << tag << "]";
    }

    #define TIME_BEGIN(TAG) \
        { std::chrono::milliseconds TIME_t0 = getCurrentTime(); const char* const TIME_tag = TAG;
    
    #define TIME_END logTime(TIME_t0, TIME_tag); }

    void timePush(
        const char* tag, const QElapsedTimer& timer,
        std::vector<qint64>& list, std::vector<QString>& tags)
    {
        const auto t = timer.elapsed();
        if (std::find(tags.begin(), tags.end(), QString(tag)) != tags.end()) 
        { 
            std::cerr << "FATAL INTERNAL ERROR: tag \"" << tag << "\" already pushed to time list\n";
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

static void showFps()
{
    Q_UNUSED(showFps);

    static const int kFpsCount = 30;
    static std::deque<double> fpsList;
    static std::chrono::milliseconds prevT(0);

    const std::chrono::milliseconds t = getCurrentTime();
    if (prevT.count() != 0)
    {
        const int delta = (t - prevT).count();
        if (delta == 0)
        {
            qWarning() << "FPS: infinite";
        }
        else
        {
            double fps = 1000.0 / delta;

            fpsList.push_back(fps);
            if (fpsList.size() > kFpsCount)
                fpsList.pop_front();
            double fpsAvg = 0;
            if (!fpsList.empty())
                fpsAvg = std::accumulate(fpsList.begin(), fpsList.end(), 0.0) / fpsList.size();

            qWarning().nospace() << "FPS: Avg: " << qPrintable(QString::number(fpsAvg, 'f', 1)) 
                << ", current: " << qPrintable(QString::number(fps, 'f', 1)) 
                << ", ms: " << delta;
        }
    }
    prevT = t;
}

//-------------------------------------------------------------------------------------------------

static const int kAlignment = 32;

static uint8_t* alignPtr(void* data)
{
    Q_UNUSED(alignPtr);

    return (uint8_t*) (
        (((uintptr_t) data) + kAlignment - 1) / kAlignment * kAlignment);
}

static uint8_t* unalignPtr(void* data)
{
    Q_UNUSED(unalignPtr);

    return (uint8_t*) (
        (17 + ((uintptr_t) data) + kAlignment - 1) / kAlignment * kAlignment);
}

//-------------------------------------------------------------------------------------------------

class YuvBuffer
{
public:
    YuvBuffer(const QSize& frameSize)
    :
        m_frameSize(frameSize)
    {
        assert(frameSize.width() % 2 == 0);
        assert(frameSize.height() % 2 == 0);

        m_yBuffer = malloc(kAlignment + m_frameSize.width() * m_frameSize.height());
        m_uBuffer = malloc(kAlignment + m_frameSize.width() * m_frameSize.height() / 4);
        m_vBuffer = malloc(kAlignment + m_frameSize.width() * m_frameSize.height() / 4);
        m_yBufferStart = alignPtr(m_yBuffer);
        m_uBufferStart = alignPtr(m_uBuffer);
        m_vBufferStart = alignPtr(m_vBuffer);
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

    ~YuvBuffer()
    {
        free(m_yBuffer);
        free(m_uBuffer);
        free(m_vBuffer);
    }

    uint8_t* y() const
    {
        return m_yBufferStart;
    }

    uint8_t* u() const
    {
        return m_uBufferStart;
    }

    uint8_t* v() const
    {
        return m_vBufferStart;
    }

private:
    const QSize m_frameSize;

    void* m_yBuffer = nullptr;
    void* m_uBuffer = nullptr;
    void* m_vBuffer = nullptr;

    uint8_t* m_yBufferStart = nullptr;
    uint8_t* m_uBufferStart = nullptr;
    uint8_t* m_vBufferStart = nullptr;
};
