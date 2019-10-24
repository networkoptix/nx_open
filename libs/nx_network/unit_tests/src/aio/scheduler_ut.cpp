#include <gtest/gtest.h>

#include <nx/network/aio/scheduler.h>
#include <nx/utils/random.h>

namespace nx::network::aio::test {

using namespace std::chrono;

class Scheduler: public testing::Test
{
protected:
    void SetUp() override
    {
        m_requiredInvokations = nx::utils::random::number(2, 5);
        m_period = milliseconds(nx::utils::random::number(50, 100));
        const int timepointCount = nx::utils::random::number(2, 10);
        for (int i = 0; i < timepointCount; ++i)
        {
            milliseconds msec(nx::utils::random::number<int>(1, m_period.count() - 1));
            while (m_schedule.find(msec) != m_schedule.end())
                msec = milliseconds(nx::utils::random::number<int>(1, m_period.count() - 1));

            m_schedule.emplace(msec);
            m_invokations[msec] = 0;
        }

        m_scheduler = std::make_unique<aio::Scheduler>(m_period, m_schedule);

        NX_DEBUG(this, "m_period: %1, m_schedule: %2, m_requiredInvokations: %4",
            m_period, containerString(m_schedule), m_requiredInvokations);
    }

    void whenStartScheduler()
    {
        m_scheduler->start(
            [this](milliseconds timepoint)
            {
                QnMutexLocker lock(&m_mutex);
                auto it = m_invokations.find(timepoint);
                ASSERT_NE(m_invokations.end(), it);
                ++it->second;
                NX_DEBUG(this, "m_invokations[%1]: %2", it->first, it->second);
                if (allScheduledInvokationsCompleted(lock))
                {
                    NX_DEBUG(this,
                        "All required invokations completed, reseting within aio thread");
                    m_scheduler.reset();
                }
            });
    }

    void thenHandlerIsScheduledRepeatedly()
    {
        QnMutexLocker lock(&m_mutex);
        do
        {
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            lock.relock();
        } while (!allScheduledInvokationsCompleted(lock));
    }

    bool allScheduledInvokationsCompleted(QnMutexLockerBase& /*lock*/) const
    {
        return std::find_if(
            m_invokations.begin(), m_invokations.end(),
            [this](const auto& iter)
            {
                return iter.second < m_requiredInvokations;
            }) == m_invokations.end();
    }

private:
    mutable QnMutex m_mutex;
    milliseconds m_period;
    std::set<milliseconds> m_schedule;
    std::unique_ptr<aio::Scheduler> m_scheduler;
    std::map<milliseconds, int /*invokations*/> m_invokations;
    int m_requiredInvokations;
};

TEST_F(Scheduler, invokes_on_schedule_repeatedly)
{
    whenStartScheduler();

    thenHandlerIsScheduledRepeatedly();
}

} // namespace nx::network::aio::test