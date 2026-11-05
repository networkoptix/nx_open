// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QPromise>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/mobile/timeline/abstract_loader_delegate.h>
#include <nx/vms/client/mobile/timeline/objects_loader.h>

namespace nx::vms::client::mobile::timeline::test {

using namespace std::chrono;

namespace {

/** Returns deferred futures, finished by the test via `requests`, or ready ones if `immediate`. */
class LoaderDelegateMock: public AbstractLoaderDelegate
{
public:
    struct Request
    {
        QnTimePeriod period;
        QPromise<MultiObjectData> promise;
    };

    virtual QFuture<MultiObjectData> load(
        const QnTimePeriod& period, milliseconds /*minimumStackDuration*/) override
    {
        if (immediate)
            return QtFuture::makeReadyValueFuture(MultiObjectData{.count = 1});

        auto& request = requests.emplace_back(Request{.period = period});
        request.promise.start();
        return request.promise.future();
    }

    /** Returns the latest request for the period starting at `startTime`. */
    Request* find(milliseconds startTime)
    {
        for (auto it = requests.rbegin(); it != requests.rend(); ++it)
        {
            if (it->period.startTime() == startTime)
                return &*it;
        }
        return nullptr;
    }

    bool immediate = false;
    std::deque<Request> requests;
};

class TestObjectsLoader: public ObjectsLoader
{
public:
    TestObjectsLoader() { setLoadDelay(1ms); }

    using ObjectsLoader::setPollingInterval;

    void initialize(milliseconds bottomBound)
    {
        setBottomBound(bottomBound);
        resetLoaderDelegate();
    }

    LoaderDelegateMock* delegate = nullptr;
    bool immediate = false;

protected:
    virtual std::unique_ptr<AbstractLoaderDelegate> createLoaderDelegate() override
    {
        auto result = std::make_unique<LoaderDelegateMock>();
        result->immediate = immediate;
        delegate = result.get();
        return result;
    }
};

/**
 * Mirrors the visible buckets the way `ObjectsList.qml` does, using only the loader signals, and
 * records the signals that are inconsistent with the loader state.
 */
class VisibleBucketsMirror
{
public:
    explicit VisibleBucketsMirror(ObjectsLoader* loader): m_loader(loader)
    {
        QObject::connect(loader,
            &ObjectsLoader::bucketsReset,
            [this]()
            {
                startTimes.clear();
                for (int i = 0; i < m_loader->count(); ++i)
                    startTimes.push_back(m_loader->bucket(i).startTimeMs);
            });

        QObject::connect(loader,
            &ObjectsLoader::scrolledIn,
            [this](int first, int count)
            {
                if (first < 0 || first > (int) startTimes.size()
                    || first + count > m_loader->count())
                {
                    errors.push_back(QString("scrolledIn(%1, %2)").arg(first).arg(count));
                    return;
                }

                for (int i = first; i < first + count; ++i)
                    startTimes.insert(startTimes.begin() + i, m_loader->bucket(i).startTimeMs);
            });

        QObject::connect(loader,
            &ObjectsLoader::scrolledOut,
            [this](int first, int count)
            {
                if (first < 0 || first + count > (int) startTimes.size())
                {
                    errors.push_back(QString("scrolledOut(%1, %2)").arg(first).arg(count));
                    return;
                }

                startTimes.erase(startTimes.begin() + first, startTimes.begin() + first + count);
            });

        QObject::connect(loader,
            &ObjectsLoader::bucketChanged,
            [this](int index)
            {
                if (index < 0 || index >= (int) startTimes.size() || index >= m_loader->count()
                    || startTimes[index] != m_loader->bucket(index).startTimeMs)
                {
                    errors.push_back(QString("bucketChanged(%1)").arg(index));
                }
            });
    }

    std::vector<qint64> startTimes;
    QStringList errors;

private:
    ObjectsLoader* const m_loader;
};

void finish(QPromise<MultiObjectData>& promise, int count)
{
    promise.addResult(MultiObjectData{.count = count});
    promise.finish();
}

} // namespace

class ObjectsLoaderTest: public ::testing::Test
{
protected:
    static constexpr milliseconds kBottomBound = 1000000ms;
    static constexpr milliseconds kBucketSize = 10s;
    static constexpr int kVisibleBuckets = 3;

    virtual void SetUp() override
    {
        m_loader = std::make_unique<TestObjectsLoader>();
        m_mirror = std::make_unique<VisibleBucketsMirror>(m_loader.get());
    }

    virtual void TearDown() override
    {
        if (m_mirror)
            ASSERT_TRUE(m_mirror->errors.empty()) << m_mirror->errors.join(", ").toStdString();
    }

    /** The window is specified in buckets from the bottom bound. */
    void givenWindow(double startBuckets,
        double durationBuckets,
        int preloadedBuckets = 0,
        milliseconds liveTime = kBottomBound + 1000 * kBucketSize)
    {
        m_loader->setPreloadedBuckets(preloadedBuckets);
        m_loader->setBucketSizeMs(kBucketSize.count());
        m_loader->setLiveTimeMs(liveTime.count());
        m_loader->setStartTimeMs(timeAt(startBuckets).count());
        m_loader->setDurationMs(qRound64(durationBuckets * kBucketSize.count()));
        m_loader->initialize(kBottomBound);

        ASSERT_TRUE(waitFor([this]() { return m_loader->count() > 0; }));
    }

    void givenVisibleBuckets(bool immediateResults = false)
    {
        m_loader->immediate = immediateResults;
        givenWindow(0, kVisibleBuckets);
        ASSERT_EQ(m_loader->count(), kVisibleBuckets);
    }

    void givenLoadingBuckets()
    {
        givenVisibleBuckets();
        ASSERT_TRUE(waitFor(
            [this]()
            {
                return m_loader->delegate
                    && (int) m_loader->delegate->requests.size() >= kVisibleBuckets;
            }));
    }

    void whenZoomed(double factor)
    {
        rememberRequests();
        const auto bucketSize = milliseconds(qRound64(m_loader->bucketSizeMs() * factor));
        m_loader->setBucketSizeMs(bucketSize.count());
        m_loader->setDurationMs((kVisibleBuckets * bucketSize).count());
    }

    void whenZoomedInBatchUpdate(double factor)
    {
        m_loader->beginBatchUpdate();
        whenZoomed(factor);
        m_loader->endBatchUpdate();
    }

    void whenZoomedAndRestoredInBatchUpdate(double factor)
    {
        const auto bucketSize = m_loader->bucketSizeMs();
        const auto duration = m_loader->durationMs();

        m_loader->beginBatchUpdate();
        whenZoomed(factor);
        m_loader->setBucketSizeMs(bucketSize);
        m_loader->setDurationMs(duration);
        m_loader->endBatchUpdate();
    }

    void whenWindowMovedTo(double startBuckets)
    {
        m_loader->setStartTimeMs(timeAt(startBuckets).count());
    }

    void whenWindowResized(double durationBuckets)
    {
        m_loader->setDurationMs(qRound64(durationBuckets * kBucketSize.count()));
    }

    void whenScrolledBy(int bucketCount)
    {
        m_loader->setStartTimeMs(m_loader->startTimeMs() + (bucketCount * kBucketSize).count());
    }

    void whenLoadingFinished(int bucketIndex, int objectCount)
    {
        finish(request(bucketIndex).promise, objectCount);
    }

    void whenLoadingFinishedInAnotherThread(int bucketIndex, int objectCount)
    {
        auto& promise = request(bucketIndex).promise;
        std::thread([&promise, objectCount]() { finish(promise, objectCount); }).join();
    }

    void whenLoadingCanceled(int bucketIndex)
    {
        auto& promise = request(bucketIndex).promise;
        promise.future().cancel();
        promise.finish();
    }

    void whenDataInvalidated(int bucketIndex)
    {
        m_invalidatedRequest = &request(bucketIndex);
        m_loader->invalidateData(bucketStart(bucketIndex).count());
    }

    void givenExpiration(milliseconds value)
    {
        m_loader->setExpirationTime(value);
        m_loader->setPollingInterval(50ms);
    }

    void whenQueuedEventsDelivered() { QCoreApplication::sendPostedEvents(); }

    void whenLoaderDestroyed()
    {
        m_mirror.reset();
        m_loader.reset();
    }

    void thenBucketsStartAt(milliseconds startTime,
        milliseconds bucketSize = kBucketSize,
        int bucketCount = kVisibleBuckets)
    {
        // `ObjectsLoader::bucket()` doesn't check the index, so `count()` must be checked first.
        const auto layoutMatches = [&]()
        {
            if (m_loader->count() != bucketCount)
                return false;

            for (int i = 0; i < bucketCount; ++i)
            {
                if (bucketStart(i) != startTime + i * bucketSize)
                    return false;
            }
            return true;
        };

        ASSERT_TRUE(waitFor(layoutMatches));
        andMirrorMatchesLoader();
    }

    void thenVisibleBuckets(double firstBucket, int bucketCount)
    {
        thenBucketsStartAt(timeAt(firstBucket), kBucketSize, bucketCount);
    }

    void andMirrorMatchesLoader()
    {
        ASSERT_EQ((int) m_mirror->startTimes.size(), m_loader->count());
        for (int i = 0; i < m_loader->count(); ++i)
            ASSERT_EQ(m_mirror->startTimes[i], m_loader->bucket(i).startTimeMs);
    }

    void andRequestsInclude(double firstBucket, int bucketCount)
    {
        ASSERT_TRUE(waitFor(
            [&]()
            {
                for (int i = 0; i < bucketCount; ++i)
                {
                    if (!m_loader->delegate->find(timeAt(firstBucket + i)))
                        return false;
                }
                return true;
            }));
    }

    void thenBucketsAreRequested(double firstBucket, int bucketCount)
    {
        andRequestsInclude(firstBucket, bucketCount);

        for (const auto& request: m_loader->delegate->requests)
        {
            ASSERT_GE(request.period.startTime(), timeAt(firstBucket));
            ASSERT_LT(request.period.startTime(), timeAt(firstBucket + bucketCount));
            ASSERT_EQ(request.period.duration(), kBucketSize);
        }
    }

    void thenNewRequestCountIs(int count)
    {
        ASSERT_EQ((int) m_loader->delegate->requests.size(), m_rememberedRequestCount + count);
    }

    void rememberRequest(int bucketIndex) { m_rememberedRequest = &request(bucketIndex); }

    void whenRememberedRequestFinished() { m_rememberedRequest->promise.finish(); }

    void andBucketIsRequestedAgain(int bucketIndex, milliseconds timeout = 1s)
    {
        ASSERT_TRUE(
            waitFor([&]() { return &request(bucketIndex) != m_rememberedRequest; }, timeout));
        thenBucketIsLoading(bucketIndex);
    }

    std::vector<QFuture<MultiObjectData>> pendingRequestFutures() const
    {
        std::vector<QFuture<MultiObjectData>> result;
        for (auto& request: m_loader->delegate->requests)
        {
            if (!request.promise.future().isFinished())
                result.push_back(request.promise.future());
        }
        return result;
    }

    void thenTopBoundIs(milliseconds value)
    {
        ASSERT_TRUE(waitFor([&]() { return m_loader->topBoundMs() == value.count(); }));
    }

    void thenPreviousRequestsAreCanceled()
    {
        for (int i = 0; i < m_rememberedRequestCount; ++i)
            ASSERT_TRUE(m_loader->delegate->requests[i].promise.isCanceled());
    }

    void thenPreviousRequestsAreNotCanceled()
    {
        for (int i = 0; i < m_rememberedRequestCount; ++i)
            ASSERT_FALSE(m_loader->delegate->requests[i].promise.isCanceled());
    }

    void andBucketsAreReloaded(milliseconds bucketSize)
    {
        ASSERT_TRUE(waitFor(
            [&]()
            {
                return (int) m_loader->delegate->requests.size()
                    >= m_rememberedRequestCount + kVisibleBuckets;
            }));

        for (int i = m_rememberedRequestCount; i < (int) m_loader->delegate->requests.size(); ++i)
            ASSERT_EQ(m_loader->delegate->requests[i].period.duration(), bucketSize);

        thenAllBucketsAreLoading();
    }

    void thenBucketIsLoading(int bucketIndex)
    {
        ASSERT_TRUE(m_loader->bucket(bucketIndex).isLoading);
    }

    void thenAllBucketsAreLoading()
    {
        for (int i = 0; i < m_loader->count(); ++i)
            thenBucketIsLoading(i);
    }

    void thenBucketIsReady(int bucketIndex, int objectCount)
    {
        ASSERT_TRUE(waitFor(
            [&]() { return m_loader->bucket(bucketIndex).state == ObjectBucket::State::Ready; }));

        const auto bucket = m_loader->bucket(bucketIndex);
        ASSERT_FALSE(bucket.isLoading);
        ASSERT_EQ(bucket.getData().count, objectCount);
    }

    void thenAllBucketsAreReady(int objectCount)
    {
        for (int i = 0; i < kVisibleBuckets; ++i)
            thenBucketIsReady(i, objectCount);
    }

    void thenBucketIsEmpty(int bucketIndex)
    {
        ASSERT_TRUE(waitFor(
            [&]() { return m_loader->bucket(bucketIndex).state == ObjectBucket::State::Empty; }));

        const auto bucket = m_loader->bucket(bucketIndex);
        ASSERT_FALSE(bucket.isLoading);
        ASSERT_EQ(bucket.state, ObjectBucket::State::Empty);
    }

    void thenInvalidatedRequestIsCanceled()
    {
        ASSERT_TRUE(m_invalidatedRequest->promise.isCanceled());
    }

    void andBucketIsReloading(int bucketIndex)
    {
        thenBucketIsLoading(bucketIndex);
        ASSERT_NE(&request(bucketIndex), m_invalidatedRequest);
    }

    void rememberRequests()
    {
        m_rememberedRequestCount = (int) m_loader->delegate->requests.size();
    }

    static milliseconds timeAt(double buckets)
    {
        return kBottomBound + milliseconds(qRound64(buckets * kBucketSize.count()));
    }

private:
    static bool waitFor(std::function<bool()> condition, milliseconds timeout = 1s)
    {
        const QDeadlineTimer deadline(timeout);
        while (!condition())
        {
            if (deadline.hasExpired())
                return false;

            QEventLoop eventLoop;
            QTimer::singleShot(10ms, &eventLoop, &QEventLoop::quit);
            eventLoop.exec();
        }
        return true;
    }

    LoaderDelegateMock::Request& request(int bucketIndex)
    {
        const auto result = m_loader->delegate->find(bucketStart(bucketIndex));
        NX_ASSERT(result);
        return *result;
    }

    milliseconds bucketStart(int bucketIndex) const
    {
        return milliseconds(m_loader->bucket(bucketIndex).startTimeMs);
    }

private:
    std::unique_ptr<TestObjectsLoader> m_loader;
    std::unique_ptr<VisibleBucketsMirror> m_mirror;
    LoaderDelegateMock::Request* m_invalidatedRequest = nullptr;
    LoaderDelegateMock::Request* m_rememberedRequest = nullptr;
    int m_rememberedRequestCount = 0;
};

TEST_F(ObjectsLoaderTest, bucketsCreated)
{
    givenVisibleBuckets();
    thenBucketsStartAt(kBottomBound);
}

TEST_F(ObjectsLoaderTest, scrolling)
{
    givenVisibleBuckets();
    whenScrolledBy(/*bucketCount*/ 2);
    thenBucketsStartAt(kBottomBound + 2 * kBucketSize);
}

TEST_F(ObjectsLoaderTest, unalignedWindowIncludesPartiallyVisibleBuckets)
{
    givenWindow(/*startBuckets*/ 10.5, /*durationBuckets*/ 3);
    thenVisibleBuckets(/*firstBucket*/ 10, /*bucketCount*/ 4);
}

TEST_F(ObjectsLoaderTest, windowEndingAtBucketBoundary)
{
    givenWindow(/*startBuckets*/ 10.5, /*durationBuckets*/ 2.5);
    thenVisibleBuckets(/*firstBucket*/ 10, /*bucketCount*/ 3);
}

TEST_F(ObjectsLoaderTest, scrollingByPartialBuckets)
{
    givenWindow(/*startBuckets*/ 10, /*durationBuckets*/ 3);

    for (const double start: {10.5, 11.0, 11.7, 13.2, 12.9, 11.1, 9.4, 9.0})
    {
        whenWindowMovedTo(start);

        const int firstBucket = (int) std::floor(start);
        thenVisibleBuckets(firstBucket, (int) std::ceil(start + 3) - firstBucket);
    }
}

TEST_F(ObjectsLoaderTest, resizingWindow)
{
    givenWindow(/*startBuckets*/ 10, /*durationBuckets*/ 3);

    whenWindowResized(5.5);
    thenVisibleBuckets(/*firstBucket*/ 10, /*bucketCount*/ 6);

    whenWindowResized(1);
    thenVisibleBuckets(/*firstBucket*/ 10, /*bucketCount*/ 1);
}

TEST_F(ObjectsLoaderTest, preloadedBucketsAreRequested)
{
    givenWindow(/*startBuckets*/ 10, /*durationBuckets*/ 3, /*preloadedBuckets*/ 2);
    thenVisibleBuckets(/*firstBucket*/ 10, /*bucketCount*/ 3);
    thenBucketsAreRequested(/*firstBucket*/ 8, /*bucketCount*/ 7);
}

TEST_F(ObjectsLoaderTest, cachedBucketsAreKeptWhenScrollingBack)
{
    givenWindow(/*startBuckets*/ 10, /*durationBuckets*/ 3);
    thenBucketsAreRequested(/*firstBucket*/ 10, /*bucketCount*/ 3);
    whenLoadingFinished(/*bucketIndex*/ 0, /*objectCount*/ 2);

    whenWindowMovedTo(12);
    thenVisibleBuckets(/*firstBucket*/ 12, /*bucketCount*/ 3);

    whenWindowMovedTo(10);
    thenVisibleBuckets(/*firstBucket*/ 10, /*bucketCount*/ 3);
    thenBucketIsReady(0, 2);
}

TEST_F(ObjectsLoaderTest, jumpWithinCacheKeepsLoadedBuckets)
{
    givenWindow(/*startBuckets*/ 10, /*durationBuckets*/ 3);
    thenBucketsAreRequested(/*firstBucket*/ 10, /*bucketCount*/ 3);
    whenLoadingFinished(/*bucketIndex*/ 0, /*objectCount*/ 2);

    whenWindowMovedTo(30);
    thenVisibleBuckets(/*firstBucket*/ 30, /*bucketCount*/ 3);
    andRequestsInclude(/*firstBucket*/ 30, /*bucketCount*/ 3);

    rememberRequests();
    whenWindowMovedTo(10);
    thenVisibleBuckets(/*firstBucket*/ 10, /*bucketCount*/ 3);
    thenBucketIsReady(0, 2);

    // Only the bucket added by resizing is requested: the others are loaded or still loading.
    whenWindowResized(4);
    andRequestsInclude(/*firstBucket*/ 13, /*bucketCount*/ 1);
    thenNewRequestCountIs(1);
}

TEST_F(ObjectsLoaderTest, topBoundIsLiveTimeUntilTopBucketIsLoaded)
{
    givenWindow(/*startBuckets*/ 10,
        /*durationBuckets*/ 3,
        /*preloadedBuckets*/ 0,
        /*liveTime*/ timeAt(12.5));
    thenBucketsAreRequested(/*firstBucket*/ 10, /*bucketCount*/ 3);
    thenTopBoundIs(timeAt(12.5));

    whenLoadingFinished(/*bucketIndex*/ 2, /*objectCount*/ 1);
    thenTopBoundIs(timeAt(13));
}

TEST_F(ObjectsLoaderTest, zoomingIn)
{
    givenLoadingBuckets();
    whenZoomed(0.5);
    thenBucketsStartAt(kBottomBound, kBucketSize / 2);
    thenPreviousRequestsAreCanceled();
    andBucketsAreReloaded(kBucketSize / 2);
}

TEST_F(ObjectsLoaderTest, zoomingOut)
{
    givenLoadingBuckets();
    whenZoomed(2);
    thenBucketsStartAt(kBottomBound, kBucketSize * 2);
    thenPreviousRequestsAreCanceled();
    andBucketsAreReloaded(kBucketSize * 2);
}

TEST_F(ObjectsLoaderTest, zoomingInBatchUpdate)
{
    givenLoadingBuckets();
    whenZoomedInBatchUpdate(2);
    thenBucketsStartAt(kBottomBound, kBucketSize * 2);
    thenPreviousRequestsAreCanceled();
    andBucketsAreReloaded(kBucketSize * 2);
}

TEST_F(ObjectsLoaderTest, batchUpdateWithUnchangedBucketSizeKeepsBuckets)
{
    givenLoadingBuckets();
    whenLoadingFinished(/*bucketIndex*/ 1, /*objectCount*/ 2);

    // The buckets are reset (and their loading canceled) synchronously in `endBatchUpdate()`.
    whenZoomedAndRestoredInBatchUpdate(2);
    thenPreviousRequestsAreNotCanceled();

    thenBucketsStartAt(kBottomBound);
    thenBucketIsReady(1, 2);
    thenBucketIsLoading(0);
}

TEST_F(ObjectsLoaderTest, bucketsLoaded)
{
    givenLoadingBuckets();
    thenAllBucketsAreLoading();

    whenLoadingFinished(/*bucketIndex*/ 1, /*objectCount*/ 2);
    thenBucketIsReady(1, 2);
    thenBucketIsLoading(0);
}

TEST_F(ObjectsLoaderTest, immediateResult)
{
    givenVisibleBuckets(/*immediateResults*/ true);
    thenAllBucketsAreReady(/*objectCount*/ 1);
}

TEST_F(ObjectsLoaderTest, canceledLoading)
{
    givenLoadingBuckets();
    whenLoadingCanceled(0);
    thenBucketIsEmpty(0);
}

TEST_F(ObjectsLoaderTest, resultFromAnotherThread)
{
    givenLoadingBuckets();
    whenLoadingFinishedInAnotherThread(/*bucketIndex*/ 0, /*objectCount*/ 3);
    thenBucketIsLoading(0); //< The result is being delivered via the loader's event queue.
    thenBucketIsReady(0, 3);
}

TEST_F(ObjectsLoaderTest, invalidationCancelsLoading)
{
    givenLoadingBuckets();
    whenDataInvalidated(0);
    thenInvalidatedRequestIsCanceled();
    andBucketIsReloading(0);
}

TEST_F(ObjectsLoaderTest, staleResultIgnored)
{
    givenLoadingBuckets();

    // The result continuation is queued, and the bucket is invalidated before it's invoked.
    whenLoadingFinishedInAnotherThread(/*bucketIndex*/ 0, /*objectCount*/ 5);
    whenDataInvalidated(0);
    whenQueuedEventsDelivered();
    thenBucketIsLoading(0);

    whenLoadingFinished(/*bucketIndex*/ 0, /*objectCount*/ 7);
    thenBucketIsReady(0, 7);
}

TEST_F(ObjectsLoaderTest, bucketRecreatedWhileLoading)
{
    givenLoadingBuckets();
    rememberRequest(0);

    // The jump is bigger than the cache, so the buckets are destroyed and created again.
    whenScrolledBy(/*bucketCount*/ 500);
    thenBucketsStartAt(kBottomBound + 500 * kBucketSize);
    whenScrolledBy(/*bucketCount*/ -500);
    thenBucketsStartAt(kBottomBound);
    andBucketIsRequestedAgain(0);

    // The canceled old request finishes now, and must not affect the new bucket.
    whenRememberedRequestFinished();
    whenQueuedEventsDelivered();
    thenBucketIsLoading(0);

    whenLoadingFinished(/*bucketIndex*/ 0, /*objectCount*/ 4);
    thenBucketIsReady(0, 4);
}

TEST_F(ObjectsLoaderTest, reloadAfterExpiration)
{
    givenExpiration(100ms);
    givenLoadingBuckets();
    rememberRequest(0);

    whenLoadingFinished(/*bucketIndex*/ 0, /*objectCount*/ 1);
    thenBucketIsReady(0, 1);

    // Expired buckets are reloaded by the loader's polling timer.
    andBucketIsRequestedAgain(0);
}

TEST_F(ObjectsLoaderTest, destructionWhileLoading)
{
    givenLoadingBuckets();
    const auto futures = pendingRequestFutures();
    ASSERT_EQ((int) futures.size(), kVisibleBuckets);

    // Pending promises are destroyed together with the delegate, canceling the requests.
    whenLoaderDestroyed();

    for (const auto& future: futures)
    {
        ASSERT_TRUE(future.isCanceled());
        ASSERT_TRUE(future.isFinished());
    }
}

} // namespace nx::vms::client::mobile::timeline::test
