#include <future>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <nx/utils/counter.h>
#include <nx/utils/random.h>

namespace nx::utils::test {

class Counter:
    public ::testing::Test
{
public:
    Counter()
    {
        m_counter = std::make_unique<utils::Counter>();
    }

    ~Counter()
    {
        if (m_decrementThread.joinable())
            m_decrementThread.join();
    }

protected:
    void increaseCounter()
    {
        m_counter->increment();
    }

    void whenDecrementedAsynchronously()
    {
        std::promise<void> started;

        m_decrementThread = std::thread(
            [this, &started]()
            {
                started.set_value();
                m_counter->decrement();
            });

        started.get_future().wait();
    }

    void whenWaitForRandomShortTime()
    {
        std::this_thread::sleep_for(
            std::chrono::microseconds(random::number<int>(0, 10)));
    }

    void thenCounterCanBeRemovedAsap()
    {
        m_counter->wait();
        m_counter.reset();
    }

private:
    std::unique_ptr<utils::Counter> m_counter;
    std::thread m_decrementThread;
};

TEST_F(Counter, can_be_removed_just_after_it_reaches_zero)
{
    increaseCounter();
    whenDecrementedAsynchronously();
    thenCounterCanBeRemovedAsap();
}

} // namespace nx::utils::test
