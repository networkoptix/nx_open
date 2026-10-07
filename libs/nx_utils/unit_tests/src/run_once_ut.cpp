// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <nx/utils/run_once.h>

#if defined(Q_OS_UNIX)
    #include <sys/resource.h>
#endif

namespace nx::utils::test {


TEST(RunOnce, withoutKey)
{
    RunOnce<void> runOnce;

    nx::Mutex mutex;
    std::atomic<int> counter = 0;

    auto worker =
        [&]()
        {
            NX_MUTEX_LOCKER lock(&mutex);
            ++counter;
        };

    auto task = *runOnce.startOnceAsync(worker);
    task.wait();
    ASSERT_EQ(1, counter);

    mutex.lock();
    for (int i = 0; i < 10; ++i)
    {
        if (auto newTask = runOnce.restartOnceAsync(worker))
        {
            ASSERT_EQ(0, i);
            task = std::move(*newTask);
        }
    }
    mutex.unlock();
    task.wait();
    ASSERT_EQ(3, counter);
}

TEST(RunOnce, withKey)
{
    std::vector<int> counters(5);

    RunOnce<int> runOnce;
    std::map<int, RunOnceFuture> tasks;
    nx::Mutex mutex;

    auto worker = [&counters, &mutex](int index)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        ++counters[index];
    };

    for (size_t i = 0; i < counters.size(); ++i)
    {
        if (auto task = runOnce.startOnceAsync(std::bind(worker, i), i))
            tasks[i] = std::move(*task);
    }
    tasks.clear(); //< wait for done

    for (size_t i = 0; i < counters.size(); ++i)
        ASSERT_EQ(1, counters[i]);

    mutex.lock();
    for (size_t i = 0; i < counters.size(); ++i)
    {
        for (int j = 0; j < 10; ++j)
        {
            if (auto task = runOnce.restartOnceAsync(std::bind(worker, i), i))
                tasks[i] = std::move(*task);
        }
    }
    mutex.unlock();
    tasks.clear(); //< wait for done
    for (size_t i = 0; i < counters.size(); ++i)
        ASSERT_EQ(3, counters[i]);
}

#if defined(Q_OS_UNIX)

// Thread creation may fail on low-memory devices (e.g. 32-bit ARM). RunOnce must report the
// failure instead of letting std::system_error escape and terminate the process - and must roll
// its state back so that a later call is able to retry.
class RunOnceUnderThreadLimit: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        rlimit prevLimit{};
        ASSERT_EQ(0, getrlimit(RLIMIT_NPROC, &prevLimit));
        // The limit counts every thread of the real user id, so any value that low makes the next
        // pthread_create() fail with EAGAIN. Already running threads are not affected.
        const rlimit tightLimit{1, prevLimit.rlim_max};
        if (setrlimit(RLIMIT_NPROC, &tightLimit) != 0)
        {
            GTEST_SKIP() << "Unable to lower RLIMIT_NPROC";
            return;
        }
        m_savedLimit = std::move(prevLimit);
    }

    virtual void TearDown() override { restoreThreadLimit(); }

    void restoreThreadLimit()
    {
        if (auto limit = std::exchange(m_savedLimit, std::nullopt); limit.has_value())
        {
            ASSERT_EQ(0, setrlimit(RLIMIT_NPROC, &*limit));
        }
    }

    /**
     * Both RunOnce flavours must behave the same way, so the check is shared. The key pack is
     * empty for RunOnce<void> and holds the single key for the keyed one.
     */
    template<typename RunOnceType, typename... Key>
    void expectFailureIsReportedAndRecoverable(RunOnceType& runOnce, const Key&... key)
    {
        std::atomic<int> counter = 0;
        auto worker = [&counter]()
        {
            ++counter;
        };

        ASSERT_FALSE(runOnce.startOnceAsync(worker, key...));
        ASSERT_FALSE(runOnce.restartOnceAsync(worker, key...));
        ASSERT_EQ(0, counter);

        restoreThreadLimit();

        auto task = runOnce.startOnceAsync(worker, key...);
        ASSERT_TRUE(task); //< The state has been rolled back, so the retry starts the task.
        task->wait();
        ASSERT_EQ(1, counter);
    }

private:
    std::optional<rlimit> m_savedLimit;
};

TEST_F(RunOnceUnderThreadLimit, withoutKey)
{
    RunOnce<void> runOnce;
    expectFailureIsReportedAndRecoverable(runOnce);
}

TEST_F(RunOnceUnderThreadLimit, withKey)
{
    RunOnce<int> runOnce;
    expectFailureIsReportedAndRecoverable(runOnce, 1);
}

#endif // defined(Q_OS_UNIX)

namespace {

/**
 * Lets a test drive the copies of a RunOnce key, which is what makes the task launch failure
 * reproducible without touching the process resource limits.
 *
 * A start copies the key into the state map, then into the task lambda, and std::async() copies it
 * once more when it moves the lambda into the shared state - the key has no move constructor, so
 * every such transfer is a copy. The first two are mandated by the standard, so parking the second
 * one leaves the caller with the state claimed and no task thread yet.
 *
 * The copy that is made to fail is the one std::async() makes, not the one the lambda capture
 * makes: an exception thrown while a capture is initialized does not reach the enclosing catch on
 * MSVC, which calls std::terminate while unwinding instead. Both are inside the try block that
 * guards the launch, so either reproduces the failure, but only the former does so portably.
 */
class CopyGate
{
public:
    /** Called by a key being copied: fails if armed, otherwise reports and waits to be let go. */
    void onCopy()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (std::exchange(m_failNextCopy, false))
            throw std::runtime_error("Gated key copy has failed");

        const int index = ++m_copyCount;
        m_copyReported.notify_all();

        if (!m_passThrough)
        {
            m_copyResumed.wait(
                lock, [this, index]() { return m_passThrough || m_resumedUpTo >= index; });
        }
    }

    /** Blocks until the given copy is reported, i.e. until its maker is parked inside it. */
    void waitForCopy(int index)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_copyReported.wait(lock, [this, index]() { return m_copyCount >= index; });
    }

    void resumeCopy(int index)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_resumedUpTo = index;
        m_copyResumed.notify_all();
    }

    /**
     * Makes the next copy throw. A copy that is already parked has passed this point, so a test
     * parked at the task lambda arms the copy std::async() makes rather than its own.
     */
    void failNextCopy()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_failNextCopy = true;
    }

    /** Resumes every parked copy and stops gating the ones made afterwards. */
    void stopGating()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_passThrough = true;
        m_copyResumed.notify_all();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_copyReported;
    std::condition_variable m_copyResumed;
    int m_copyCount = 0;
    int m_resumedUpTo = 0;
    bool m_failNextCopy = false;
    bool m_passThrough = false;
};

/** A RunOnce key that reports its copies to a CopyGate. Ordered by id, so copies compare equal. */
class GatedKey
{
public:
    GatedKey(int id, CopyGate* gate): m_id(id), m_gate(gate) {}

    GatedKey(const GatedKey& other): m_id(other.m_id), m_gate(other.m_gate) { m_gate->onCopy(); }

    GatedKey& operator=(const GatedKey&) = delete; //< std::map never assigns its keys.

    bool operator<(const GatedKey& other) const { return m_id < other.m_id; }

    QString toString() const { return QString::number(m_id); }

private:
    int m_id;
    CopyGate* m_gate;
};

static constexpr int kStateMapCopy = 1;
static constexpr int kTaskLambdaCopy = 2;

/**
 * Starts a task whose launch fails: parks the start at the task lambda, arms the copy that
 * std::async() makes to throw, and returns what startOnceAsync() reported. Runs the start on its
 * own thread because parking blocks the caller.
 */
std::optional<RunOnceFuture> startWithFailingLaunch(
    RunOnce<GatedKey>& runOnce, const std::function<void()>& task, int id, CopyGate* gate)
{
    std::optional<RunOnceFuture> result;
    std::thread starter([&]() { result = runOnce.startOnceAsync(task, GatedKey(id, gate)); });

    gate->waitForCopy(kStateMapCopy);
    gate->resumeCopy(kStateMapCopy);
    gate->waitForCopy(kTaskLambdaCopy);
    gate->failNextCopy();
    gate->stopGating();
    starter.join();

    return result;
}

} // namespace

/** A restart requested while a task is being started has to be coalesced into one extra run. */
TEST(RunOnceLaunchFailure, coalescesAConcurrentRestart)
{
    CopyGate gate;
    RunOnce<GatedKey> runOnce;
    std::mutex taskMutex;
    std::atomic<int> counter = 0;
    auto worker = [&]()
    {
        std::unique_lock<std::mutex> lock(taskMutex);
        ++counter;
    };

    std::unique_lock<std::mutex> taskLock(taskMutex); //< Hold the task until both calls are done.

    std::optional<RunOnceFuture> startResult;
    std::thread starter(
        [&]() { startResult = runOnce.startOnceAsync(worker, GatedKey(1, &gate)); });

    gate.waitForCopy(kStateMapCopy);
    gate.resumeCopy(kStateMapCopy);
    gate.waitForCopy(kTaskLambdaCopy);
    gate.stopGating(); //< Let the launch succeed this time.
    starter.join();
    ASSERT_TRUE(startResult);

    for (int i = 0; i < 10; ++i)
        ASSERT_FALSE(runOnce.restartOnceAsync(worker, GatedKey(1, &gate)));

    taskLock.unlock();
    startResult->wait();
    ASSERT_EQ(2, counter); //< The ten restart requests are coalesced into a single extra run.
}

/**
 * A task launch that fails must be reported to the caller instead of terminating the process, and
 * must leave the state such that a later call is able to retry. VMS-63010.
 */
TEST(RunOnceLaunchFailure, isReportedAndRecoverable)
{
    CopyGate gate;
    RunOnce<GatedKey> runOnce;
    std::atomic<int> counter = 0;
    auto worker = [&counter]()
    {
        ++counter;
    };

    ASSERT_FALSE(startWithFailingLaunch(runOnce, worker, 1, &gate));
    ASSERT_EQ(0, counter);

    auto task = runOnce.startOnceAsync(worker, GatedKey(1, &gate));
    ASSERT_TRUE(task); //< The state has been rolled back, so the retry starts the task.
    task->wait();
    ASSERT_EQ(1, counter);
}

/**
 * A restart requested while another thread is still starting a task must not be lost when that
 * start then fails: the state is claimed and the thread is created under one lock, so the restart
 * either observes a running task or starts one itself.
 */
TEST(RunOnceLaunchFailure, doesNotDiscardAConcurrentRestart)
{
    CopyGate gate;
    RunOnce<GatedKey> runOnce;
    std::atomic<int> counter = 0;
    auto worker = [&counter]()
    {
        ++counter;
    };

    std::optional<RunOnceFuture> startResult;
    std::thread starter(
        [&]() { startResult = runOnce.startOnceAsync(worker, GatedKey(1, &gate)); });

    gate.waitForCopy(kStateMapCopy);
    gate.resumeCopy(kStateMapCopy);
    gate.waitForCopy(kTaskLambdaCopy); //< State claimed, task thread not created yet.

    std::optional<RunOnceFuture> restartResult;
    std::promise<void> restartEntered;
    std::thread restarter(
        [&]()
        {
            restartEntered.set_value();
            restartResult = runOnce.restartOnceAsync(worker, GatedKey(1, &gate));
        });

    // Wait for the restarting thread to be running, so that the start below really does have to
    // serialize with it. There is deliberately no check that the restart is still blocked by then:
    // a thread that is about to block cannot be observed, and any wait long enough to guess at it
    // would be unreliable on a loaded machine. The outcome assertion at the end is the real check.
    restartEntered.get_future().wait();

    gate.failNextCopy();
    gate.stopGating();

    starter.join();
    restarter.join();

    ASSERT_FALSE(startResult); //< The launch failed, as arranged.
    if (restartResult)
        restartResult->wait();
    ASSERT_EQ(1, counter) << "the restart request has been discarded by the failed start";
}

/** A start that fails for one key must leave the states of the other keys alone. */
TEST(RunOnceLaunchFailure, doesNotDisturbOtherKeys)
{
    CopyGate runningGate;
    runningGate.stopGating(); //< The key that keeps running is never parked.
    CopyGate failingGate;

    RunOnce<GatedKey> runOnce;
    std::mutex taskMutex;
    std::atomic<int> counter = 0;
    auto worker = [&]()
    {
        std::unique_lock<std::mutex> lock(taskMutex);
        ++counter;
    };

    std::unique_lock<std::mutex> taskLock(taskMutex); //< Hold the task of the running key.

    auto running = runOnce.startOnceAsync(worker, GatedKey(2, &runningGate));
    ASSERT_TRUE(running);

    ASSERT_FALSE(startWithFailingLaunch(runOnce, worker, 1, &failingGate));

    // The other key is still claimed, so its restart is coalesced rather than starting a task.
    ASSERT_FALSE(runOnce.restartOnceAsync(worker, GatedKey(2, &runningGate)))
        << "the failed start has dropped the state of another key";

    taskLock.unlock();
    running->wait();
    ASSERT_EQ(2, counter); //< The first run plus the coalesced restart.
}

/**
 * A task that throws must not leave the state claimed, which would stall every later call. The
 * exception has to stay observable through the returned future.
 */
TEST(RunOnceTaskFailure, doesNotStallTheState)
{
    CopyGate gate;
    gate.stopGating();
    RunOnce<GatedKey> runOnce;
    std::atomic<int> counter = 0;
    auto thrower = [&counter]()
    {
        ++counter;
        throw std::runtime_error("task failure");
    };

    auto failed = runOnce.startOnceAsync(thrower, GatedKey(1, &gate));
    ASSERT_TRUE(failed);
    failed->wait();
    ASSERT_EQ(1, counter);
    ASSERT_THROW(failed->get(), std::runtime_error); //< Still reported to whoever asks.

    auto task = runOnce.startOnceAsync([&counter]() { ++counter; }, GatedKey(1, &gate));
    ASSERT_TRUE(task) << "the state is still claimed by the task that has thrown";
    task->wait();
    ASSERT_EQ(2, counter);
}

TEST(RunOnceTaskFailure, doesNotStallTheStateWithoutKey)
{
    RunOnce<void> runOnce;
    std::atomic<int> counter = 0;
    auto thrower = [&counter]()
    {
        ++counter;
        throw std::runtime_error("task failure");
    };

    auto failed = runOnce.startOnceAsync(thrower);
    ASSERT_TRUE(failed);
    failed->wait();
    ASSERT_THROW(failed->get(), std::runtime_error);

    auto task = runOnce.startOnceAsync([&counter]() { ++counter; });
    ASSERT_TRUE(task) << "the state is still claimed by the task that has thrown";
    task->wait();
    ASSERT_EQ(2, counter);
}

/** The task may throw something that is not a std::exception, and must be handled the same way. */
TEST(RunOnceTaskFailure, handlesANonStandardException)
{
    RunOnce<void> runOnce;
    std::atomic<int> counter = 0;
    auto thrower = [&counter]()
    {
        ++counter;
        throw 42; //< Not derived from std::exception, so the catch-all arm has to take it.
    };

    auto failed = runOnce.startOnceAsync(thrower);
    ASSERT_TRUE(failed);
    failed->wait();
    ASSERT_THROW(failed->get(), int); //< Still reported to whoever asks.

    auto task = runOnce.startOnceAsync([&counter]() { ++counter; });
    ASSERT_TRUE(task) << "the state is still claimed by the task that has thrown";
    task->wait();
    ASSERT_EQ(2, counter);
}

} // namespace nx::utils::test
