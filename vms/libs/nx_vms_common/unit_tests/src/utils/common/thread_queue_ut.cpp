// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <thread>

#include <utils/common/threadqueue.h>

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds kTimeout{50};
static constexpr int kQueueSize = 100;

// -----------------------------------------------------------------------------------------------
// Tests with fixture 1
// -----------------------------------------------------------------------------------------------

struct TestParams
{
    using QueueT = QnSafeQueue<int>;

    mutable std::shared_ptr<QueueT> queue = {};
    std::function<void(QueueT&)> prepare = {};
    int expectedSize = -1;
    std::string name = {};

    QueueT& get() const
    {
        queue = std::make_shared<QueueT>(kQueueSize);
        prepare(*queue.get());
        return *queue.get();
    }
};

void PrintTo(const TestParams& params, std::ostream* os)
{
    *os << "Name: " << params.name << ", Expected size: " << params.expectedSize;
}

struct Configurer
{
    std::string name;
    std::function<void(TestParams::QueueT&)> configure;
    Configurer(std::string n, std::function<void(TestParams::QueueT&)> func)
        :
        name(std::move(n)),
        configure(std::move(func))
    {
    }
};

void PrintTo(const Configurer& conf, std::ostream* os)
{
    *os << "Configurer: " << conf.name;
}

using ParamsWithConfigurer = std::tuple<TestParams, Configurer>;

class ThreadQueueFixture : public ::testing::TestWithParam<ParamsWithConfigurer>
{
public:
    TestParams::QueueT& getQueue()
    {
        auto& queue = std::get<0>(GetParam()).get();
        std::get<1>(GetParam()).configure(queue);
        return queue;
    }

    int expectedSize() const
    {
        return std::get<0>(GetParam()).expectedSize;
    }

    static std::string PrintToString(const testing::TestParamInfo<ParamsWithConfigurer>& info)
    {
        const auto& [params, configurer] = info.param;
        return params.name + "_" + configurer.name;
    }

    template <int Count>
    static void init(TestParams::QueueT& queue)
    {
        for (int i = 1; i <= Count; ++i)
            queue.push(i);
    }

    template <int Count>
    static void initWrapped(TestParams::QueueT& queue)
    {
        for (int i = -(kQueueSize / 2) + 1; i <= kQueueSize / 2; ++i)
            queue.push(i);

        int val;
        for (int i = 0; i < kQueueSize / 2; ++i)
            queue.pop(val);

        for (int i = kQueueSize / 2 + 1; i <= Count; ++i)
            queue.push(i);
    }
};

INSTANTIATE_TEST_SUITE_P(
    ThreadQueue,
    ThreadQueueFixture,
    ::testing::Combine(
        ::testing::Values(
        // Empty queue.
        TestParams{
            .prepare = ThreadQueueFixture::init<0>,
            .expectedSize = 0,
            .name = "empty",
        },
        // Usual half full queue.
        TestParams{
            .prepare = ThreadQueueFixture::init<kQueueSize / 2>,
            .expectedSize = kQueueSize / 2,
            .name = "half_full",
        },
        // Usual full queue.
        TestParams{
            .prepare = ThreadQueueFixture::init<kQueueSize>,
            .expectedSize = kQueueSize,
            .name = "full",
        },
        // Wrapped around queue with gap.
        TestParams{
            .prepare = ThreadQueueFixture::initWrapped<kQueueSize - kQueueSize / 2>,
            .expectedSize = kQueueSize - kQueueSize / 2,
            .name = "wrapped_w_gap",
        },
        // Wrapped around queue without gap (full).
        TestParams{
            .prepare = ThreadQueueFixture::initWrapped<kQueueSize>,
            .expectedSize = kQueueSize,
            .name = "wrapped_wo_gap",
        }),
        ::testing::Values(
            Configurer("none", [](auto&){ /* do nothing */ }),
            Configurer("doubled", [](auto& queue){ queue.setMaxSize(kQueueSize * 2); })
        )
    ),
    ThreadQueueFixture::PrintToString
);

// Basic checks.
TEST_P(ThreadQueueFixture, Basic)
{
    auto& queue = getQueue();

    auto access = queue.lock();
    ASSERT_EQ(access.size(), expectedSize());

    if (access.size())
    {
        ASSERT_EQ(access.front(), 1);
        ASSERT_EQ(access.last(), expectedSize());
    }
    else
    {
        ASSERT_EQ(access.front(), int());
        ASSERT_EQ(access.last(), int());
    }

    // Same object should be returned.
    ASSERT_EQ(access.front(), access.at(0));
    ASSERT_EQ(&access.front(), &access.at(0));
    ASSERT_EQ(access.last(), access.at(access.size() - 1));
    ASSERT_EQ(&access.last(), &access.at(access.size() - 1));

    for (int i = 1; i <= access.size(); ++i)
        ASSERT_EQ(access.at(i - 1), i);

    for (int i = 1; i <= expectedSize(); ++i)
    {
        ASSERT_EQ(access.front(), i);
        ASSERT_TRUE(access.popFront());
        ASSERT_EQ(access.size(), expectedSize() - i);
    }
}

// RandomAccess move ctor transfers mutex ownership correctly.
TEST_P(ThreadQueueFixture, MutexOwnershipTransfer)
{
    auto& queue = getQueue();

    auto access1 = queue.lock();
    EXPECT_EQ(access1.size(), expectedSize());

    auto access2 = std::move(access1);
    EXPECT_EQ(access2.size(), expectedSize());
}

// Pack does nothing if no gaps present.
TEST_P(ThreadQueueFixture, NothingToPack)
{
    auto& queue = getQueue();

    auto access = queue.lock();
    access.pack();

    ASSERT_EQ(access.size(), expectedSize());
    for (int i = 0; i < access.size(); ++i)
        ASSERT_EQ(access.at(i), i + 1);
}

// Pack removes gaps correctly.
TEST_P(ThreadQueueFixture, Pack)
{
    auto& queue = getQueue();

    auto access = queue.lock();
    const int initialSize = access.size();
    for (int i = 0; i < access.size(); ++i)
    {
        if (i % 2 != 0)
            ASSERT_TRUE(access.setAt(int(), i));
    }

    access.pack();
    ASSERT_EQ(access.size(), initialSize / 2 + initialSize % 2);

    for (int i = 0; i < initialSize; ++i)
    {
        if (i % 2 == 0)
        {
            ASSERT_EQ(i + 1, access.front());
            ASSERT_TRUE(access.popFront());
        }
    }
    ASSERT_EQ(access.size(), 0);
}

// Clear works as intended.
TEST_P(ThreadQueueFixture, Clear)
{
    auto& queue = getQueue();

    auto access = queue.lock();
    access.clear();
    ASSERT_EQ(access.size(), 0);
}

// Setting values check.
TEST_P(ThreadQueueFixture, Set)
{
    auto& queue = getQueue();

    auto access = queue.lock();
    for (int i = 0; i < access.size(); ++i)
    {
        const int value = -2 * i;
        ASSERT_TRUE(access.setAt(value, i));
        ASSERT_EQ(access.at(i), value);
    }

    if (access.size())
    {
        ASSERT_TRUE(access.setAt(-42, 0));
        ASSERT_EQ(access.front(), -42);
        ASSERT_TRUE(access.setAt(-42, access.size() - 1));
        ASSERT_EQ(access.last(), -42);
    }
}

// Removing one element in the middle of the queue.
TEST_P(ThreadQueueFixture, RemoveOneInMiddle)
{
    if (!expectedSize())
        return;

    auto& queue = getQueue();

    auto access = queue.lock();
    const int middle = access.size() / 2;
    ASSERT_TRUE(access.removeAt(middle));
    ASSERT_EQ(access.size(), expectedSize() - 1);

    for (int i = 1; i <= access.size(); ++i)
        ASSERT_EQ(access.at(i - 1), i - 1 < middle ? i : i + 1);
}

// Removing all queue elements forward.
TEST_P(ThreadQueueFixture, RemoveAllForward)
{
    auto& queue = getQueue();

    auto access = queue.lock();
    const int initialSize = access.size();
    for (int i = 1; i <= initialSize; ++i)
    {
        ASSERT_TRUE(access.removeAt(0));
        ASSERT_EQ(access.size(), initialSize - i);

        for (int j = 0; j < access.size(); ++j)
            ASSERT_EQ(access.at(j), j + i + 1);
    }
    ASSERT_EQ(access.size(), 0);
}

// Removing all queue elements backward.
TEST_P(ThreadQueueFixture, RemoveAllBackward)
{
    auto& queue = getQueue();

    auto access = queue.lock();
    const int initialSize = access.size();
    for (int i = initialSize; i >= 1; --i)
    {
        ASSERT_TRUE(access.removeAt(access.size() - 1));
        ASSERT_EQ(access.size(), i - 1);

        for (int j = 0; j < access.size(); ++j)
            ASSERT_EQ(access.at(j), j + 1);
    }
    ASSERT_EQ(access.size(), 0);
}

// Concurrent RandomAccess test (read-only under contention).
TEST_P(ThreadQueueFixture, ConcurrentReadersRandomAccess)
{
    auto& queue = getQueue();

    const int count = expectedSize() * expectedSize();
    constexpr int kThreads = 8;

    std::atomic<int> matchCount{0};

    auto reader =
        [&]()
        {
            for (int i = 0; i < count; ++i)
            {
                const auto access = queue.lock();
                const int idx = i % access.size();
                if (access.at(idx) == idx + 1)
                    matchCount++;
            }
        };

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i)
        threads.emplace_back(reader);

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(matchCount.load(), count * kThreads);
}

// DetachDataByCondition test.
TEST_P(ThreadQueueFixture, DetachDataByCondition)
{
    auto& queue = getQueue();

    queue.detachDataByCondition([](const int val, const auto&){ return val % 2 != 0; });

    auto access = queue.lock();
    ASSERT_EQ(access.size(), expectedSize());
    for (int i = 1; i <= access.size(); ++i)
        ASSERT_EQ(access.at(i - 1), (i % 2 != 0) ? int() : i);
}

// Access to out-of-bounds indexes return T().
TEST_P(ThreadQueueFixture, OobAccess)
{
    auto& queue = getQueue();
    auto access = queue.lock();
    for (int i = expectedSize(); i < kQueueSize; ++i)
    {
        EXPECT_EQ(access.at(i), int()) << i;
        ASSERT_FALSE(access.setAt(42, i));
        ASSERT_FALSE(access.removeAt(i));
    }
}

// -----------------------------------------------------------------------------------------------
// Tests with fixture 2
// -----------------------------------------------------------------------------------------------

struct PopTestParams
{
    std::string name;
    int expectedValue = -1;
    bool expectedResult = false;
    std::chrono::milliseconds popTimeout = kTimeout * 2;
    std::chrono::milliseconds sleepTimeout = kTimeout;

    std::function<void(std::shared_ptr<QnSafeQueue<int>>*)> afterSleep;
};

void PrintTo(const PopTestParams& params, std::ostream* os)
{
    *os << "Expected Value: " << params.expectedValue << ", result: " << params.expectedResult;
}

class ThreadQueuePopFixture : public ::testing::TestWithParam<PopTestParams>
{
public:
    static std::string PrintToString(const testing::TestParamInfo<PopTestParams>& info)
    {
        return info.param.name + "_pop_should_" + (info.param.expectedResult ? "succeed" : "fail");
    }
};

INSTANTIATE_TEST_SUITE_P(
    ThreadQueue,
    ThreadQueuePopFixture,
    ::testing::Values(
        // Termination.
        PopTestParams{
            .name = "terminate",
            .expectedValue = -1,
            .expectedResult = false,
            .afterSleep = [](auto* queue){ (*queue)->setTerminated(true); },
        },
        // Push.
        PopTestParams{
            .name = "push",
            .expectedValue = 42,
            .expectedResult = true,
            .afterSleep = [](auto* queue){ (*queue)->push(42); },
        },
        // Timeout.
        PopTestParams{
            .name = "timeout",
            .expectedValue = -1,
            .expectedResult = false,
            .popTimeout = kTimeout,
            .sleepTimeout = kTimeout,
            .afterSleep = [](auto*){ /* od nothing*/ },
        }
    ),
    ThreadQueuePopFixture::PrintToString
);

// Pop unblocks as intended.
TEST_P(ThreadQueuePopFixture, PopUnblocking)
{
    auto queue = std::make_shared<QnSafeQueue<int>>();

    const auto start = std::chrono::steady_clock::now();
    std::atomic<bool> popReturned = false;
    std::thread t(
        [params = GetParam(), &popReturned, q = queue]()
        {
            int val = -1;
            const bool result = q->pop(val, params.popTimeout); //< Wait for long time.

            EXPECT_EQ(result, params.expectedResult);
            EXPECT_EQ(val, params.expectedValue);

            popReturned = true;
        });

    std::this_thread::sleep_for(GetParam().sleepTimeout);
    GetParam().afterSleep(&queue);

    t.join();
    EXPECT_TRUE(popReturned.load());

    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    EXPECT_GE(duration.count(), GetParam().sleepTimeout.count() * 0.8);
}

// -----------------------------------------------------------------------------------------------
// Tests without fixture
// -----------------------------------------------------------------------------------------------

// Stress test under high contention using push/pop.
TEST(ThreadQueue, HighContentionPushPop)
{
    QnSafeQueue<int> queue(kQueueSize);

    constexpr int kThreads = 8;
    constexpr int kOperations = 10000;
    std::atomic<int> totalConsumed{0};

    auto producer =
        [&]()
        {
            for (int i = 1; i <= kOperations; ++i)
            {
                ASSERT_TRUE(queue.push(i));
                std::this_thread::yield();
            }
        };

    auto consumer =
        [&]()
        {
            while (totalConsumed < kThreads * kOperations)
            {
                int val = -1;
                if (queue.pop(val, 10ms))
                {
                    ASSERT_GT(val, 0);
                    totalConsumed++;
                }
                std::this_thread::yield();
            }
        };

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(producer);
        threads.emplace_back(consumer);
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(totalConsumed, kThreads * kOperations);
}

// Stress test under high contention using push and RandomAccess::popFront.
TEST(ThreadQueue, HighContentionPushPopFront)
{
    QnSafeQueue<int> queue(kQueueSize);

    constexpr int kThreads = 8;
    constexpr int kOperations = 10000;
    std::atomic<int> totalConsumed{0};

    auto producer =
        [&]()
        {
            for (int i = 1; i <= kOperations; ++i)
            {
                ASSERT_TRUE(queue.push(i));
                std::this_thread::yield();
            }
        };

    auto consumer =
        [&]()
        {
            while (totalConsumed < kThreads * kOperations)
            {
                {
                    auto access = queue.lock();
                    if (access.size() > 0)
                    {
                        ASSERT_GT(access.front(), 0);
                        ASSERT_TRUE(access.popFront());
                        totalConsumed++;
                    }
                }
                std::this_thread::yield();
            }
        };

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(producer);
        threads.emplace_back(consumer);
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(totalConsumed, kThreads * kOperations);
}

// Max size increase/decrease.
TEST(ThreadQueue, MaxSize)
{
    QnSafeQueue<int> queue(kQueueSize);
    for (int i = 1; i <= kQueueSize; ++i)
        queue.push(i);

    ASSERT_EQ(queue.maxSize(), kQueueSize);

    queue.setMaxSize(kQueueSize / 2);
    ASSERT_EQ(queue.maxSize(), kQueueSize / 2);

    queue.setMaxSize(kQueueSize * 2);
    ASSERT_EQ(queue.maxSize(), kQueueSize * 2);

    auto access = queue.lock();
    for (int i = 1; i <= access.size(); ++i)
        ASSERT_EQ(access.at(i - 1), i);
}

// Push to wrapped full queue results in correct behavior.
TEST(ThreadQueue, PushToWrappedFull)
{
    QnSafeQueue<int> queue(kQueueSize);
    ThreadQueueFixture::initWrapped<kQueueSize>(queue);

    queue.push(kQueueSize + 1);
    auto access = queue.lock();
    for (int i = 1; i <= access.size(); ++i)
        ASSERT_EQ(access.at(i - 1), i);
}

// Push to terminated queue fails.
TEST(ThreadQueue, PushToTerminated)
{
    QnSafeQueue<int> queue;
    ASSERT_TRUE(queue.push(1));
    ASSERT_EQ(queue.lock().size(), 1);

    queue.setTerminated(true);
    ASSERT_FALSE(queue.push(2));
    ASSERT_EQ(queue.lock().size(), 1);
}

// Access by negative index or to empty queue return T().
TEST(ThreadQueue, InvalidIndex)
{
    QnSafeQueue<int> queue;
    auto access = queue.lock();

    ASSERT_FALSE(access.popFront());

    for (int i: {-1, 0})
    {
        ASSERT_EQ(access.at(i), int());
        ASSERT_FALSE(access.setAt(42, i));
        ASSERT_FALSE(access.removeAt(i));
    }
}
