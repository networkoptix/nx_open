#include <gtest/gtest.h>

#include <thread>

#include <nx/utils/random.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace utils {
namespace test {

class SyncQueue:
    public ::testing::Test
{
protected:
    nx::utils::thread pushAsync(
        std::vector<int> values,
        std::chrono::milliseconds delay = std::chrono::milliseconds(0))
    {
        return nx::utils::thread(
            [this, values = std::move(values), delay]()
            {
                for (const auto& value: values)
                {
                    if (delay.count())
                        std::this_thread::sleep_for(delay);

                    queue.push(value);
                }
            });
    }

    int popSum(std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
    {
        int total(0);
        while (auto value = queue.pop(timeout))
            total += *value;

        return total;
    }

    nx::utils::SyncQueue<int> queue;
};

TEST_F(SyncQueue, SinglePushPop)
{
    auto pusher = pushAsync({1, 2, 3, 4, 5});
    ASSERT_EQ(popSum(), 15);
    pusher.join();
}

TEST_F(SyncQueue, MultiPushPop)
{
    auto pusher1 = pushAsync({1, 2, 3, 4, 5});
    auto pusher2 = pushAsync({5, 5, 5});
    auto pusher3 = pushAsync({3, 3, 3, 3});

    ASSERT_EQ(popSum(), 42);

    pusher1.join();
    pusher2.join();
    pusher3.join();
}


TEST_F(SyncQueue, TimedPop)
{
    const std::chrono::milliseconds kSmallDelay(100);
    const std::chrono::hours kLongDelay(1);

    const auto syncPushAndPop =
        [&](int value)
        {
            auto pusher = pushAsync({value}, kSmallDelay);
            const auto popedValue = queue.pop(kLongDelay);
            pusher.join();
            return popedValue && popedValue == value;
        };

    ASSERT_FALSE(static_cast<bool>(queue.pop(kSmallDelay)));
    syncPushAndPop(1);
    ASSERT_FALSE(static_cast<bool>(queue.pop(kSmallDelay)));
    syncPushAndPop(2);
    ASSERT_FALSE(static_cast<bool>(queue.pop(kSmallDelay)));
}

TEST_F(SyncQueue, conditional_pop)
{
    for (int i = 1; i < 8; ++i)
        queue.push(i);

    ASSERT_EQ(4, queue.popIf([](int val) { return val > 3; }));
    ASSERT_EQ(2, queue.popIf([](int val) { return val > 1; }));
    ASSERT_EQ(1, queue.popIf([](int val) { return val >= 1; }));
}

TEST_F(SyncQueue, conditional_pop_blocks_if_no_element_satisfies_condition)
{
    for (int i = 1; i < 8; ++i)
        queue.push(i);

    std::atomic<int> limit(8);

    std::thread t(
        [this, &limit]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            limit = 4;
            queue.retestPopIfCondition();
        });

    ASSERT_EQ(4, queue.popIf([&limit](int val) { return val >= limit.load(); }));

    t.join();
}

//-------------------------------------------------------------------------------------------------

class SyncQueueTermination:
    public ::testing::Test
{
public:
    SyncQueueTermination()
    {
    }

    ~SyncQueueTermination()
    {
        deinit();
    }

protected:
    void givenReaderThreadBlockedOnPopFromQueue()
    {
        initializeReaders(1U);
    }

    void givenMultipleReadersThreadBlockedOnPopFromQueue()
    {
        initializeReaders(nx::utils::random::number<std::size_t>(2, 10));
    }

    void havingAskedRandomReaderToTerminate()
    {
        terminateReader(m_readers.begin()->first);
    }

    void havingAskedReaderToTerminate()
    {
        ASSERT_EQ(1U, m_readers.size());
        terminateAllReaders();
    }

    void verifyThatReaderHasStopped()
    {
        // If reader did not stop we will hang in destructor, so doing nothing here.
    }

    void verifyThatOtherReadersAreWorking()
    {
        QnMutex mutex;
        QnWaitCondition condition;
        boost::optional<QueueReaderId> disturbedReader;

        setOnElementPoppedHandler(
            [&mutex, &condition, &disturbedReader](QueueReaderId readerId, int /*value*/)
            {
                QnMutexLocker lock(&mutex);
                disturbedReader = readerId;
                condition.wakeAll();
            });

        while (!m_readers.empty())
        {
            m_syncQueue.push(1);
            QnMutexLocker lock(&mutex);
            while (!disturbedReader)
                condition.wait(lock.mutex());
            terminateReader(*disturbedReader);
            disturbedReader.reset();
        }
    }

private:
    struct ReaderContext
    {
        nx::utils::thread thread;
    };

    using Readers = std::map<QueueReaderId, ReaderContext>;
    using OnElementReadHandler =
        nx::utils::MoveOnlyFunc<void(QueueReaderId /*readerId*/, int /*value*/)>;

    Readers m_readers;
    nx::utils::SyncQueue<int> m_syncQueue;
    OnElementReadHandler m_onElementReadHandler;

    void initializeReaders(std::size_t readerCount)
    {
        for (std::size_t i = 0; i < readerCount; ++i)
        {
            m_readers.emplace(
                i,
                ReaderContext{nx::utils::thread(
                    std::bind(&SyncQueueTermination::readThreadMain, this, i))});
        }
    }

    void readThreadMain(std::size_t readerId)
    {
        for (;;)
        {
            boost::optional<int> x = m_syncQueue.pop(readerId);
            if (!x)
                break;
            if (m_onElementReadHandler)
                m_onElementReadHandler(readerId, *x);
        }
    }

    void terminateAllReaders()
    {
        for (Readers::iterator it = m_readers.begin(); it != m_readers.end();)
        {
            m_syncQueue.addReaderToTerminatedList(it->first);
            it->second.thread.join();
            it = m_readers.erase(it);
        }
    }

    void terminateReader(QueueReaderId readerId)
    {
        auto it = m_readers.find(readerId);
        ASSERT_NE(m_readers.end(), it);
        m_syncQueue.addReaderToTerminatedList(it->first);
        it->second.thread.join();
        m_readers.erase(it);
    }

    void setOnElementPoppedHandler(OnElementReadHandler onElementReadHandler)
    {
        m_onElementReadHandler = std::move(onElementReadHandler);
    }

    void deinit()
    {
        ASSERT_TRUE(m_readers.empty());
    }
};

TEST_F(SyncQueueTermination, SingleReaderTermination)
{
    givenReaderThreadBlockedOnPopFromQueue();
    havingAskedReaderToTerminate();
    verifyThatReaderHasStopped();
}

TEST_F(SyncQueueTermination, OtherReadersKeepWorkingAfterOneHasBeenTerminated)
{
    givenMultipleReadersThreadBlockedOnPopFromQueue();
    havingAskedRandomReaderToTerminate();
    verifyThatOtherReadersAreWorking();
}

} // namespace test
} // namespace utils
} // namespace nx
