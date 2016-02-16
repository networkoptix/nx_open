#ifndef __CALL_COUNTER_H__
#define __CALL_COUNTER_H__

#include <map>
#include <cstdint>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include <utils/common/singleton.h>
#include <utils/common/log.h>

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
    QnCallCounter(int reportPeriodMs);
    ~QnCallCounter();

    void count(QString functionName);

private:
    void startReporter();

private:
    std::map<QString, CallInfo> m_callInfo;

    std::thread m_thread;
    std::mutex m_mutex;

    std::condition_variable m_cond;
    std::atomic<bool> m_needStop;

    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_lastTriggerTime;
    std::chrono::milliseconds m_reportPeriod;
};

//#define QN_CALL_COUNT

#ifdef QN_CALL_COUNT
#   define QnCallCountStart(val) \
        auto qnCallCounterPtr = std::unique_ptr<QnCallCounter>(new QnCallCounter(val))
#
#   define QnCallCount() \
        do \
        { \
            if (QnCallCounter::instance()) \
                QnCallCounter::instance()->count(Q_FUNC_INFO); \
        } \
        while(0)
#else
#   define QnCallCountStart(val) 
#   define QnCallCount() 
#endif

#endif // __CALL_COUNTER_H__