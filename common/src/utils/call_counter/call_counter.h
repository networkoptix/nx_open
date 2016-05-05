#ifndef __CALL_COUNTER_H__
#define __CALL_COUNTER_H__

#include <map>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include <nx/utils/singleton.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/thread.h>


class QnCallCounter : public Singleton<QnCallCounter>
{
private:
    struct CallInfo
    {
        int64_t totalCalls;
        int64_t lastTotalCalls;

        CallInfo(int64_t totalCalls = 0, int64_t lastTotalCalls = 0)
            : totalCalls(totalCalls),
              lastTotalCalls(lastTotalCalls)
        {}
    };

public:
    QnCallCounter(std::chrono::milliseconds reportPeriod);
    ~QnCallCounter();

    void incrementCallCount(QString functionName);

private:
    void startReporter();

private:
    std::map<QString, CallInfo> m_callInfo;

    nx::utils::thread m_thread;
    std::mutex m_mutex;

    std::condition_variable m_cond;
    bool m_needStop;

    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_lastTriggerTime;
    std::chrono::milliseconds m_reportPeriod;
};

//#define QN_CALL_COUNT

#ifdef QN_CALL_COUNT
#   define QnCallCountStart(val) \
        auto qnCallCounterPtr = std::unique_ptr<QnCallCounter>(new QnCallCounter(val))
#
#   define QnIncrementCallCount() \
        QnCallCounter::instance()->incrementCallCount(Q_FUNC_INFO) 
#else
#   define QnCallCountStart(val) 
#   define QnIncrementCallCount() 
#endif

#endif // __CALL_COUNTER_H__