// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "objects_loader.h"

#include <deque>
#include <memory>
#include <optional>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QFutureWatcher>
#include <QtCore/QTimer>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/media/chunk_provider.h>
#include <nx/vms/client/core/timeline/backends/aggregating_proxy_backend.h>
#include <nx/vms/client/core/timeline/backends/analytics_backend.h>
#include <nx/vms/client/core/timeline/backends/bookmarks_backend.h>
#include <nx/vms/client/core/timeline/backends/caching_proxy_backend.h>
#include <recording/time_period.h>

#include "analytics_loader_delegate.h"
#include "bookmark_loader_delegate.h"
#include "motion_loader_delegate.h"

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;
using namespace nx::vms::client::core::timeline;

// ------------------------------------------------------------------------------------------------
// ObjectsLoader::InternalBucket

struct ObjectsLoader::InternalBucket: public ObjectBucket
{
    std::unique_ptr<QFutureWatcher<MultiObjectData>> runningLoadingTask;
    QDeadlineTimer expiration{Qt::VeryCoarseTimer};

    InternalBucket(InternalBucket&&) = default;
    InternalBucket& operator=(InternalBucket&&) = default;

    InternalBucket(const qint64& startTimeMs): ObjectBucket{.startTimeMs = startTimeMs} {}

    ~InternalBucket()
    {
        if (runningLoadingTask)
            runningLoadingTask->cancel();
    }
};

// ------------------------------------------------------------------------------------------------
// ObjectsLoader::Private

struct ObjectsLoader::Private
{
    ObjectsLoader* const q;

    // Visualized time period.
    QnTimePeriod window;

    // Visualized time period rounded to bucket boundaries.
    QnTimePeriod roundedWindow;

    // Time period matching the `cache` container.
    QnTimePeriod cacheWindow;

    // Visualized time period rounded to bucket boundaries and expanded by
    // `preloadedBuckets` * `bucketSize` in each direction.
    QnTimePeriod expandedWindow;

    // Objects type to load.
    ObjectsType objectsType{ObjectsType::motion};

    milliseconds bucketSize{};
    int preloadedBuckets = 2;
    int desiredCacheSize = 100;

    milliseconds liveTime{milliseconds::max()};
    milliseconds internalTopBound{milliseconds::max()};

    seconds expirationTime = 30s;

    int maxObjectsPerBucket = 100;

    milliseconds minimumStackDuration = 500ms;

    std::optional<milliseconds> bottomBound;

    // The topmost bucket's end time if the bucket has loaded objects and extends past live,
    // or the live time otherwise.
    milliseconds topBound{milliseconds::max()};

    QPointer<core::ChunkProvider> chunkProvider;
    BackendPtr<ObjectTrackList> analyticsBackend;
    BackendPtr<QnCameraBookmarkList> bookmarksBackend;
    std::unique_ptr<AbstractLoaderDelegate> loaderDelegate;

    QTimer pollingTimer;
    nx::utils::PendingOperation loadOp;
    bool forceUpdate = false;

    using BucketList = std::deque<InternalBucket>;
    BucketList cache;

    QnTimePeriodList objectChunks;

    // Visible part of `cache`.
    int firstVisibleBucket = 0;
    int visibleBucketCount = 0;

    // Visible part of `cache` expanded by `preloadedBuckets` in each direction.
    int firstLoadedBucket = 0;
    int loadedBucketCount = 0;

    Private(ObjectsLoader* q):
        q(q),
        loadOp([this]() { loadData(); }, 200ms)
    {
        loadOp.setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);

        pollingTimer.callOnTimeout(q, [this]() { requestLoad(); });
        pollingTimer.start(1s);
    }

    int maximumCacheSize() const
    {
        return std::max(desiredCacheSize, loadedBucketCount);
    }

    void clear()
    {
        const bool hadVisibleBuckets = visibleBucketCount > 0;

        cache.clear();
        cacheWindow = {};
        roundedWindow = {};
        expandedWindow = {};
        internalTopBound = liveTime;
        firstLoadedBucket = firstVisibleBucket = 0;
        loadedBucketCount = visibleBucketCount = 0;

        if (hadVisibleBuckets)
            emit q->bucketsReset();

        if (!objectChunks.empty())
        {
            objectChunks = {};
            emit q->objectChunksChanged();
        }

        if (topBound != liveTime)
        {
            topBound = liveTime;
            emit q->topBoundChanged();
        }
    }

    milliseconds calculateTopBound() const
    {
        const auto croppedStartMs = (internalTopBound - bucketSize).count();
        const auto croppedBucketIt = find(croppedStartMs);

        const bool hasCroppedBucketAtLive = croppedBucketIt != cache.cend()
            && croppedBucketIt->state == ObjectBucket::State::Ready;

        return hasCroppedBucketAtLive ? internalTopBound : liveTime;
    }

    void setTopBound(milliseconds value)
    {
        if (topBound == value)
            return;

        topBound = value;
        emit q->topBoundChanged();
    }

    void updateTopBound()
    {
        setTopBound(calculateTopBound());
    }

    void handleWindowChanged()
    {
        // If the visible window is empty, just clear everything.
        if (!window.isValid() || bucketSize == 0ms || !bottomBound)
        {
            clear();
            return;
        }

        // Calculate the window encompassing fully or partially visible buckets.
        const auto start = window.startTime()
            - ((window.startTime() - *bottomBound) % bucketSize);

        const auto endRemainder = (window.endTime() - *bottomBound) % bucketSize;

        const auto end = endRemainder > 0ms
            ? (window.endTime() - endRemainder + bucketSize)
            : window.endTime();

        const auto newRoundedWindow = QnTimePeriod::fromInterval(start, end);
        if (!newRoundedWindow.isValid())
        {
            clear();
            return;
        }

        const bool fullyReset = !newRoundedWindow.intersects(roundedWindow) || cache.empty();

        // Calculate the window encompassing visible buckets and preloaded buckets at each side.
        const auto extraTimeBehind = bucketSize * preloadedBuckets;
        const auto extraTimeAhead = newRoundedWindow.endTime() < liveTime ? extraTimeBehind : 0ms;

        const auto newExpandedWindow = QnTimePeriod::fromInterval(
            newRoundedWindow.startTime() - extraTimeBehind,
            newRoundedWindow.endTime() + extraTimeAhead);

        const int totalBucketCount = (int) std::ceil(
            (double) (liveTime - *bottomBound).count() / bucketSize.count());

        const auto roundedLiveTime = *bottomBound + bucketSize * totalBucketCount;
        internalTopBound = std::max(newRoundedWindow.endTime(), roundedLiveTime);

        if (roundedWindow == newRoundedWindow && expandedWindow == newExpandedWindow)
            return;

        // Notify about scrolled out buckets if not fully reset.
        if (!fullyReset)
        {
            if (newRoundedWindow.startTime() > roundedWindow.startTime())
            {
                const auto shift = newRoundedWindow.startTime() - roundedWindow.startTime();
                const int count = shift / bucketSize;
                NX_ASSERT((shift % bucketSize) == 0ms);
                firstVisibleBucket += count;
                visibleBucketCount -= count;
                emit q->scrolledOut(0, count);
            }

            if (newRoundedWindow.endTime() < roundedWindow.endTime())
            {
                const auto shift = roundedWindow.endTime() - newRoundedWindow.endTime();
                const int count = shift / bucketSize;
                NX_ASSERT((shift % bucketSize) == 0ms);
                visibleBucketCount -= count;
                emit q->scrolledOut(visibleBucketCount, count);
            }
        }

        const auto oldRoundedWindow = roundedWindow;

        roundedWindow = newRoundedWindow;
        expandedWindow = newExpandedWindow;
        visibleBucketCount = roundedWindow.duration() / bucketSize;
        loadedBucketCount = expandedWindow.duration() / bucketSize;

        NX_ASSERT((roundedWindow.duration() % bucketSize) == 0ms);
        NX_ASSERT((expandedWindow.duration() % bucketSize) == 0ms);

        // Fill the bucket cache with empty buckets where needed, crop the cache if needed.
        const int maxCacheSize = maximumCacheSize();
        if (cache.empty()
            || ((expandedWindow.endTime() - cacheWindow.startTime()) / bucketSize) > maxCacheSize
            || ((cacheWindow.endTime() - expandedWindow.startTime()) / bucketSize) > maxCacheSize)
        {
            if (!objectChunks.empty())
            {
                objectChunks.clear();
                emit q->objectChunksChanged();
            }

            cache.clear();

            std::generate_n(std::back_inserter(cache), loadedBucketCount,
                [this, timeMs = expandedWindow.startTimeMs]() mutable
                {
                    const auto timestampMs = timeMs;
                    timeMs += bucketSize.count();
                    return InternalBucket(timestampMs);
                });

            firstLoadedBucket = 0;
            firstVisibleBucket = preloadedBuckets;
        }
        else if (expandedWindow.startTime() < cacheWindow.startTime())
        {
            // Insertion at the front.
            const auto insertedTime = cacheWindow.startTime() - expandedWindow.startTime();
            const int insertedCount = insertedTime / bucketSize;

            NX_ASSERT(insertedTime % bucketSize == 0ms);
            NX_ASSERT(insertedCount <= maxCacheSize);

            // Crop back if needed.
            if ((int) cache.size() + insertedCount > maxCacheSize)
                cache.erase(cache.begin() + (maxCacheSize - insertedCount), cache.end());

            // Insert empty buckets with initialized timestamps.
            std::generate_n(std::front_inserter(cache), insertedCount,
                [this, timeMs = cacheWindow.startTimeMs]() mutable
                {
                    timeMs -= bucketSize.count();
                    return InternalBucket(timeMs);
                });

            firstLoadedBucket = 0;
            firstVisibleBucket = preloadedBuckets;
        }
        else if (expandedWindow.endTimeMs() > cacheWindow.endTimeMs())
        {
            // Insertion at the back.
            const auto insertedTime = expandedWindow.endTime() - cacheWindow.endTime();
            const int insertedCount = insertedTime / bucketSize;

            NX_ASSERT(insertedTime % bucketSize == 0ms);
            NX_ASSERT(insertedCount <= maxCacheSize);

            // Crop front if needed.
            if ((int) cache.size() + insertedCount > maxCacheSize)
                cache.erase(cache.begin(), cache.end() - (maxCacheSize + insertedCount));

            // Insert empty buckets with initialized timestamps.
            std::generate_n(std::back_inserter(cache), insertedCount,
                [this, timeMs = cacheWindow.endTimeMs()]() mutable
                {
                    const auto timestampMs = timeMs;
                    timeMs += bucketSize.count();
                    return InternalBucket(timestampMs);
                });

            firstLoadedBucket = (int) cache.size() - loadedBucketCount;
            firstVisibleBucket = firstLoadedBucket + preloadedBuckets;
        }
        else
        {
            firstLoadedBucket = (expandedWindow.startTime() - cacheWindow.startTime()) / bucketSize;
            firstVisibleBucket = (roundedWindow.startTime() - cacheWindow.startTime()) / bucketSize;
        }

        if (NX_ASSERT(!cache.empty()))
        {
            cacheWindow = QnTimePeriod::fromInterval(
                cache.front().startTimeMs,
                cache.back().startTimeMs + bucketSize.count());

            const auto oldSize = objectChunks.size();
            objectChunks = objectChunks.intersected(cacheWindow);

            if (oldSize != objectChunks.size())
                emit q->objectChunksChanged();
        }

        NX_ASSERT(firstVisibleBucket + visibleBucketCount <= (int) cache.size());

        if (fullyReset)
        {
            emit q->bucketsReset();
        }
        else
        // Notify about scrolled in buckets if not fully reset.
        {
            if (roundedWindow.startTime() < oldRoundedWindow.startTime())
            {
                const auto shift = oldRoundedWindow.startTime() - roundedWindow.startTime();
                const int count = shift / bucketSize;
                NX_ASSERT((shift % bucketSize) == 0ms);
                emit q->scrolledIn(0, count);
            }

            if (roundedWindow.endTime() > oldRoundedWindow.endTime())
            {
                const auto shift = roundedWindow.endTime() - oldRoundedWindow.endTime();
                const int count = shift / bucketSize;
                NX_ASSERT((shift % bucketSize) == 0ms);
                emit q->scrolledIn(visibleBucketCount - count, count);
            }
        }

        updateTopBound();
        requestLoad();
    }

    void requestLoad()
    {
        if (loaderDelegate && loadedBucketCount > 0)
            loadOp.requestOperation();
    }

    void loadData()
    {
        if (!loaderDelegate)
            return;

        auto start = expandedWindow.startTime();
        auto bucketIt = cache.begin() + firstLoadedBucket;

        for (int i = 0; i < loadedBucketCount; ++i, ++bucketIt, start += bucketSize)
        {
            if (bucketIt->runningLoadingTask)
                continue;

            if (forceUpdate || bucketIt->expiration.hasExpired())
            {
                const QnTimePeriod period(start, bucketSize);
                update(bucketIt, loaderDelegate->load(period, minimumStackDuration));
            }
        }

        forceUpdate = false;
    }

    void update(BucketList::iterator bucketIt, QFuture<MultiObjectData>&& future)
    {
        if (future.isCanceled())
        {
            handleBucketLoaded(bucketIt, std::nullopt);
        }
        else if (future.isFinished() && NX_ASSERT(future.isValid() && future.isResultReadyAt(0)))
        {
            handleBucketLoaded(bucketIt, future.result());
        }
        else
        {
            bucketIt->runningLoadingTask.reset(new QFutureWatcher<MultiObjectData>());

            connect(bucketIt->runningLoadingTask.get(), &QFutureWatcher<MultiObjectData>::finished,
                nx::utils::guarded(bucketIt->runningLoadingTask.get(),
                    [this, startTimeMs = bucketIt->startTimeMs]()
                    {
                        const auto bucketIt = find(startTimeMs);
                        if (!NX_ASSERT(bucketIt != cache.end()
                            && bucketIt->runningLoadingTask
                            && bucketIt->runningLoadingTask->isFinished()))
                        {
                            return;
                        }

                        auto future = bucketIt->runningLoadingTask->future();
                        auto objectsData = future.isValid() && future.isResultReadyAt(0)
                            ? std::optional<MultiObjectData>(future.takeResult())
                            : std::nullopt;

                        bucketIt->runningLoadingTask.release()->deleteLater();
                        handleBucketLoaded(bucketIt, std::move(objectsData));
                    }));

            bucketIt->runningLoadingTask->setFuture(future);
            bucketIt->isLoading = true;

            const int index = std::distance(cache.begin(), bucketIt) - firstVisibleBucket;
            if (index >= 0 && index < visibleBucketCount)
                emit q->bucketChanged(index);
        }
    }

    void handleBucketLoaded(
        BucketList::iterator bucketIt, std::optional<MultiObjectData> objectsData)
    {
        bucketIt->isLoading = false;
        bucketIt->runningLoadingTask.reset();
        bucketIt->data = objectsData;
        bucketIt->state = (objectsData && objectsData->count > 0)
            ? ObjectBucket::State::Ready
            : ObjectBucket::State::Empty;

        bucketIt->expiration.setRemainingTime(expirationTime, Qt::VeryCoarseTimer);

        updateObjectChunks(QnTimePeriod{bucketIt->startTimeMs, q->bucketSizeMs()},
            bucketIt->getData().objectChunks);

        const int index = std::distance(cache.begin(), bucketIt) - firstVisibleBucket;
        if (index >= 0 && index < visibleBucketCount)
            emit q->bucketChanged(index);

        if (bucketIt->startTimeMs == (internalTopBound - bucketSize).count())
            updateTopBound();
    }

    void updateObjectChunks(const QnTimePeriod& bucketPeriod, const QnTimePeriodList& chunks)
    {
        if (objectChunks.empty() && chunks.empty())
            return;

        const bool wasEmpty = objectChunks.empty();
        objectChunks.excludeTimePeriod(bucketPeriod);

        if (!chunks.empty())
        {
            const auto insertTo = std::upper_bound(objectChunks.begin(), objectChunks.end(),
                bucketPeriod.startTimeMs);

            const auto simplified = chunks.simplified();
            objectChunks.insert(insertTo, simplified.cbegin(), simplified.cend());
        }

        if (!wasEmpty || !objectChunks.empty())
            emit q->objectChunksChanged();
    }

    static auto findBucket(auto&& sortedBuckets, qint64 startTimeMs)
    {
        const auto it = std::lower_bound(sortedBuckets.begin(), sortedBuckets.end(), startTimeMs,
            [](const InternalBucket& bucket, qint64 startTimeMs)
            {
                return bucket.startTimeMs < startTimeMs;
            });

        return (it != sortedBuckets.end() && it->startTimeMs == startTimeMs)
            ? it
            : sortedBuckets.end();
    }

    BucketList::iterator find(qint64 startTimeMs)
    {
        return findBucket(cache, startTimeMs);
    }

    BucketList::const_iterator find(qint64 startTimeMs) const
    {
        return findBucket(cache, startTimeMs);
    }

    void initializeLoaderDelegate()
    {
        loaderDelegate.reset();
        if (!chunkProvider)
            return;

        switch (objectsType)
        {
            case ObjectsType::motion:
                loaderDelegate.reset(new MotionLoaderDelegate(chunkProvider, maxObjectsPerBucket));
                break;

            case ObjectsType::bookmarks:
                loaderDelegate.reset(new BookmarkLoaderDelegate(ensureBookmarksBackend(),
				    maxObjectsPerBucket));
                break;

            case ObjectsType::analytics:
                loaderDelegate.reset(new AnalyticsLoaderDelegate(chunkProvider,
                    ensureAnalyticsBackend(), maxObjectsPerBucket));
                break;
        }
    }

    void setBottomBound(std::optional<milliseconds> value)
    {
        if (bottomBound == value)
            return;

        if (bottomBound && value && !cache.empty())
        {
            // Don't update bottomBound if some tiles are loaded to avoid tile recalculation
            // if the archive disk space is low and the archive is getting frequently clipped.
            // Consider improving this logic in the future, e.g. adjust bottomBound in
            // current bucketSize steps.
            return;
        }

        clear();
        bottomBound = value;
        emit q->bottomBoundChanged();

        handleWindowChanged();
    }

    void setBottomBoundMs(qint64 value)
    {
        constexpr qint64 kNoTimeValue = -1;
        if (value != kNoTimeValue)
            setBottomBound(milliseconds(value));
        else
            setBottomBound(std::nullopt);
    }

    const BackendPtr<ObjectTrackList>& ensureAnalyticsBackend()
    {
        if (analyticsBackend || !NX_ASSERT(chunkProvider))
            return analyticsBackend;

        analyticsBackend =
            std::make_shared<CachingProxyBackend<ObjectTrackList>>(
                std::make_shared<AggregatingProxyBackend<ObjectTrackList>>(
                    std::make_shared<AnalyticsBackend>(chunkProvider->resource())));

        return analyticsBackend;
    }

    const BackendPtr<QnCameraBookmarkList>& ensureBookmarksBackend()
    {
        if (bookmarksBackend || !NX_ASSERT(chunkProvider))
            return bookmarksBackend;

        const auto cachingBackend = std::make_shared<CachingProxyBackend<QnCameraBookmarkList>>(
            std::make_shared<AggregatingProxyBackend<QnCameraBookmarkList>>(
                std::make_shared<BookmarksBackend>(chunkProvider->resource())));

        // Bookmarks can be modified in the past, therefore the cache blocks are always expirable.
        cachingBackend->setAgeThreshold(std::nullopt);

        bookmarksBackend = cachingBackend;
        return bookmarksBackend;
    }
};

// ------------------------------------------------------------------------------------------------
// ObjectsLoader

ObjectsLoader::ObjectsLoader(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
}

ObjectsLoader::~ObjectsLoader()
{
    // Required here for forward-declared scoped pointer destruction.
}

ObjectsLoader::ObjectsType ObjectsLoader::objectsType() const
{
    return d->objectsType;
}

void ObjectsLoader::setObjectsType(ObjectsType value)
{
    if (d->objectsType == value)
        return;

    d->clear();
    d->objectsType = value;
    d->initializeLoaderDelegate();
    emit objectsTypeChanged();

    d->handleWindowChanged();
}

bool ObjectsLoader::isSynchronous() const
{
    return d->objectsType == ObjectsType::motion;
}

core::ChunkProvider* ObjectsLoader::chunkProvider() const
{
    return d->chunkProvider;
}

void ObjectsLoader::setChunkProvider(core::ChunkProvider* value)
{
    if (d->chunkProvider == value)
        return;

    d->clear();
    d->chunkProvider = value;
    d->analyticsBackend.reset();
    d->bookmarksBackend.reset();
    d->initializeLoaderDelegate();
    emit chunkProviderChanged();

    d->bottomBound = std::nullopt;
    emit bottomBoundChanged();

    if (!d->chunkProvider)
        return;

    connect(d->chunkProvider, &core::ChunkProvider::resourceChanged, this,
        [this]()
        {
            d->clear();
            d->analyticsBackend.reset();
            d->bookmarksBackend.reset();
            d->initializeLoaderDelegate();
            d->handleWindowChanged();
        });

    connect(d->chunkProvider, &core::ChunkProvider::motionFilterChanged, this,
        [this]()
        {
            if (d->objectsType != ObjectsType::motion)
                return;

            d->clear();
            d->handleWindowChanged();
        });

    connect(d->chunkProvider, &core::ChunkProvider::periodsUpdated, this,
        [this](Qn::TimePeriodContent type)
        {
            if ((d->objectsType == ObjectsType::motion && type == Qn::MotionContent)
                || (d->objectsType == ObjectsType::analytics && type == Qn::AnalyticsContent))
            {
                d->forceUpdate = true;
                d->requestLoad();
            }
        });

    connect(d->chunkProvider, &core::ChunkProvider::bottomBoundChanged, this,
        [this]() { d->setBottomBoundMs(d->chunkProvider->bottomBound()); });

    d->setBottomBoundMs(d->chunkProvider->bottomBound());
}

QVariant ObjectsLoader::bottomBoundMs() const
{
    return d->bottomBound
        ? QVariant::fromValue(d->bottomBound->count())
        : QVariant{};
}

qint64 ObjectsLoader::topBoundMs() const
{
    return d->topBound.count();
}

milliseconds ObjectsLoader::startTime() const
{
    return d->window.startTime();
}

qint64 ObjectsLoader::startTimeMs() const
{
    return startTime().count();
}

void ObjectsLoader::setStartTime(milliseconds value)
{
    if (startTime() == value)
        return;

    d->window.setStartTime(value);
    emit startTimeChanged();

    d->handleWindowChanged();
}

void ObjectsLoader::setStartTimeMs(qint64 value)
{
    setStartTime(milliseconds{value});
}

milliseconds ObjectsLoader::duration() const
{
    return d->window.duration();
}

qint64 ObjectsLoader::durationMs() const
{
    return duration().count();
}

void ObjectsLoader::setDuration(milliseconds value)
{
    if (duration() == value || !NX_ASSERT(value >= 0ms))
        return;

    d->window.setDuration(value);
    emit durationChanged();

    d->handleWindowChanged();
}

void ObjectsLoader::setDurationMs(qint64 value)
{
    setDuration(milliseconds{value});
}

qint64 ObjectsLoader::liveTimeMs() const
{
    return d->liveTime.count();
}

void ObjectsLoader::setLiveTimeMs(qint64 value)
{
    if (d->liveTime.count() == value)
        return;

    d->liveTime = milliseconds(value);
    emit liveTimeChanged();

    // We don't need to update `internalTopBound` etc. until we scroll the window,
    // and that is handled in `handleWindowChanged`.
    d->updateTopBound();
}

milliseconds ObjectsLoader::bucketSize() const
{
    return d->bucketSize;
}

qint64 ObjectsLoader::bucketSizeMs() const
{
    return bucketSize().count();
}

void ObjectsLoader::setBucketSize(milliseconds value)
{
    if (value < 0ms)
        value = 0ms;

    if (d->bucketSize == value)
        return;

    d->clear();
    d->bucketSize = value;
    emit bucketSizeChanged();

    d->handleWindowChanged();
}

void ObjectsLoader::setBucketSizeMs(qint64 value)
{
    setBucketSize(milliseconds{value});
}

int ObjectsLoader::preloadedBuckets() const
{
    return d->preloadedBuckets;
}

void ObjectsLoader::setPreloadedBuckets(int value)
{
    if (d->preloadedBuckets == value || !NX_ASSERT(value >= 0))
        return;

    d->preloadedBuckets = value;
    emit preloadedBucketsChanged();

    d->requestLoad();
}

int ObjectsLoader::cacheSize() const
{
    return d->desiredCacheSize;
}

void ObjectsLoader::setCacheSize(int value)
{
    if (d->desiredCacheSize == value || !NX_ASSERT(value >= 0))
        return;

    d->desiredCacheSize = value;
    emit cacheSizeChanged();

    // If desired cache size is smaller than current cache size, the cache isn't cropped
    // immediately. It will be cropped after a next window change instead.
}

seconds ObjectsLoader::expirationTime() const
{
    return d->expirationTime;
}

void ObjectsLoader::setExpirationTime(seconds value)
{
    if (d->expirationTime == value)
        return;

    d->expirationTime = value;
    emit expirationTimeChanged();

    if (d->expirationTime > 0s)
        d->requestLoad();
}

int ObjectsLoader::expirationSeconds() const
{
    return expirationTime().count();
}

void ObjectsLoader::setExpirationSeconds(int value)
{
    setExpirationTime(seconds(value));
}

int ObjectsLoader::maxObjectsPerBucket() const
{
    return d->maxObjectsPerBucket;
}

void ObjectsLoader::setMaxObjectsPerBucket(int value)
{
    constexpr int kMinimumMaxObjectsPerBucket = 10;
    value = std::max(value, kMinimumMaxObjectsPerBucket);

    if (d->maxObjectsPerBucket == value)
        return;

    d->clear();
    d->maxObjectsPerBucket = value;
    d->initializeLoaderDelegate();
    emit maxObjectsPerBucketChanged();

    d->handleWindowChanged();
}

milliseconds ObjectsLoader::minimumStackDuration() const
{
    return d->minimumStackDuration;
}

qint64 ObjectsLoader::minimumStackDurationMs() const
{
    return minimumStackDuration().count();
}

void ObjectsLoader::setMinimumStackDuration(milliseconds value)
{
    if (d->minimumStackDuration == value)
        return;

    d->clear();
    d->minimumStackDuration = value;
    emit minimumStackDurationChanged();

    d->handleWindowChanged();
}

void ObjectsLoader::setMinimumStackDurationMs(qint64 value)
{
    setMinimumStackDuration(milliseconds(value));
}

QnTimePeriodList ObjectsLoader::objectChunks() const
{
    return d->objectChunks;
}

int ObjectsLoader::count() const
{
    return d->visibleBucketCount;
}

ObjectBucket ObjectsLoader::bucket(int index) const
{
    return d->cache[d->firstVisibleBucket + index];
}

void ObjectsLoader::registerQmlTypes()
{
    qRegisterMetaType<ObjectData>();
    qRegisterMetaType<MultiObjectData>();
    qRegisterMetaType<ObjectBucket>();

    qmlRegisterUncreatableType<ObjectData>("nx.vms.client.mobile.timeline", 1, 0, "ObjectData",
        "ObjectData is not a creatable type");

    qmlRegisterUncreatableType<MultiObjectData>("nx.vms.client.mobile.timeline", 1, 0, "MultiObjectData",
        "MultiObjectData is not a creatable type");

    qmlRegisterUncreatableType<ObjectBucket>("nx.vms.client.mobile.timeline", 1, 0,
        "ObjectBucket", "ObjectBucket is not a creatable type");

    qmlRegisterType<ObjectsLoader>("nx.vms.client.mobile.timeline", 1, 0, "ObjectsLoader");
}

} // namespace timeline
} // namespace nx::vms::client::mobile
