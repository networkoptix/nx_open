#include "call_counter.h"

QnCallCounter::QnCallCounter(std::chrono::milliseconds reportPeriod)
    : m_needStop(false),
      m_reportPeriod(reportPeriod)
{
    startReporter();
}

QnCallCounter::~QnCallCounter()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_needStop = true;
    }
    m_cond.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}

void QnCallCounter::incrementCallCount(QString functionName)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_callInfo.find(functionName) == m_callInfo.cend())
        m_callInfo.emplace(std::move(functionName), CallInfo());
    else
        ++m_callInfo[std::move(functionName)].totalCalls;
}

void QnCallCounter::startReporter()
{
    m_thread = nx::utils::thread(
        [this]
        {
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                m_startTime = std::chrono::high_resolution_clock::now();
                m_lastTriggerTime = std::chrono::high_resolution_clock::now();
            }

            while (true)
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_cond.wait_for(lk, m_reportPeriod, 
                                [this] 
                                { 
                                    if (m_needStop)
                                        return true; 

                                    auto timeSinceLastReport =
                                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                                    std::chrono::high_resolution_clock::now() -
                                                    m_lastTriggerTime);

                                    if (timeSinceLastReport < m_reportPeriod)
                                        return false;
                                    
                                    return true;
                                });
                if (m_needStop)
                    break;

                std::chrono::milliseconds upTime =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::high_resolution_clock::now() -
                                m_startTime);

                auto totalPeriods = (double)upTime.count() / m_reportPeriod.count();
                m_lastTriggerTime = std::chrono::high_resolution_clock::now();

                for (auto it = m_callInfo.begin(); it != m_callInfo.end(); ++it)
                {
                    auto callsAtLastPeriod = it->second.totalCalls - 
                                             it->second.lastTotalCalls;

                    auto callsPerPeriod = (double)it->second.totalCalls / totalPeriods;

                    NX_LOG(lit("(CALL REPORT) Functon: %1. Total calls: %2. Calls per %3 ms: %4. Calls during last %5 ms: %6.")
                               .arg(it->first)
                               .arg(it->second.totalCalls)
                               .arg(m_reportPeriod.count())
                               .arg(callsPerPeriod)
                               .arg(m_reportPeriod.count())
                               .arg(callsAtLastPeriod),
                           cl_logDEBUG1);

                    it->second.lastTotalCalls = it->second.totalCalls;
                }
            }
        });
}