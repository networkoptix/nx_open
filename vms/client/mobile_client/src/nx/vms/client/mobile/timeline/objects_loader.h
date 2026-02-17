// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

#include <recording/time_period_list.h>
#include <nx/utils/impl_ptr.h>

#include "objects_bucket.h"

Q_MOC_INCLUDE("nx/vms/client/core/media/chunk_provider.h")

namespace nx::vms::client::core { class ChunkProvider; }

namespace nx::vms::client::mobile {
namespace timeline {

/**
 * A data model for ObjectList displayed in DataTimeline, loading and providing timeline objects
 * of a specified type - bookmarks, motion or analytics object tracks (`objectsType` property)
 * for a specified time window (`startTimeMs` and `endTimeMs` properties).
 *
 * The whole time domain is deduced from the specified chunk provider (`chunkProvider property)
 * as the time period from the beginning of the archive and until the current time. The beginning
 * can be queried as `bottomBoundMs` property.
 *
 * The whole time domain, starting from its bottom, is subdivided into logical temporal "buckets"
 * of a specified size (`bucketSizeMs` property), and then the data for the buckets intersecting
 * with the current window is loaded (plus extra buckets on each side of the window, specified in
 * `preloadedBuckets` property).
 *
 * When the window position changes, those buckets that are scrolled
 * out of view are not immediately discarded, but cached instead (see `cacheSize` property).
 * When the window size and/or bucket size changes, the whole cache is discarded.
 *
 * The model data is logically represented by the buckets intersected with the current time window,
 * further called the *visible buckets*. It is accessed via `count()` and `bucket()` methods.
 *
 * Model structure changes - when the subset of visible buckets changes - are reported via the
 * `bucketsReset()`, `scrolledIn()` and `scrolledOut()` signals.
 *
 * Model data changes - when the content of a *visible bucket* changes - are reported via the
 * `bucketChanged()` signal.
 */
class ObjectsLoader: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    Q_PROPERTY(ObjectsType objectsType READ objectsType WRITE setObjectsType
        NOTIFY objectsTypeChanged)
    Q_PROPERTY(core::ChunkProvider* chunkProvider READ chunkProvider WRITE setChunkProvider
        NOTIFY chunkProviderChanged)
    Q_PROPERTY(bool synchronous READ isSynchronous NOTIFY objectsTypeChanged)
    Q_PROPERTY(QVariant bottomBoundMs READ bottomBoundMs NOTIFY bottomBoundChanged)
    Q_PROPERTY(qint64 topBoundMs READ topBoundMs NOTIFY topBoundChanged)
    Q_PROPERTY(qint64 startTimeMs READ startTimeMs WRITE setStartTimeMs NOTIFY startTimeChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs WRITE setDurationMs NOTIFY durationChanged)
    Q_PROPERTY(qint64 liveTimeMs READ liveTimeMs WRITE setLiveTimeMs NOTIFY liveTimeChanged)
    Q_PROPERTY(qint64 bucketSizeMs READ bucketSizeMs WRITE setBucketSizeMs
        NOTIFY bucketSizeChanged)
    Q_PROPERTY(int preloadedBuckets READ preloadedBuckets WRITE setPreloadedBuckets
        NOTIFY preloadedBucketsChanged)
    Q_PROPERTY(int cacheSize READ cacheSize WRITE setCacheSize NOTIFY cacheSizeChanged)
    Q_PROPERTY(int expirationSeconds READ expirationSeconds WRITE setExpirationSeconds
        NOTIFY expirationTimeChanged)
    Q_PROPERTY(int maxObjectsPerBucket READ maxObjectsPerBucket WRITE setMaxObjectsPerBucket
        NOTIFY maxObjectsPerBucketChanged)
    Q_PROPERTY(qint64 minimumStackDurationMs READ minimumStackDurationMs
        WRITE setMinimumStackDurationMs NOTIFY minimumStackDurationChanged)

    // Virtual "output" chunks.
    Q_PROPERTY(QnTimePeriodList objectChunks READ objectChunks NOTIFY objectChunksChanged)

public:
    enum class ObjectsType
    {
        motion,
        analytics,
        bookmarks
    };
    Q_ENUM(ObjectsType)

public:
    explicit ObjectsLoader(QObject* parent = nullptr);
    virtual ~ObjectsLoader() override;

    /**
     * Selected object type to load.
     */
    ObjectsType objectsType() const;
    void setObjectsType(ObjectsType value);
    bool isSynchronous() const;

    /**
     * Chunk provider to get native chunks from.
     * Provides to the loader:
     *    - the camera resource;
     *    - the archive beginning (an origin for the time domain division into temporal buckets);
     *    - motion content as is;
     *    - analytics chunks as a hint of where the analytics object tracks are present,
     *      so the loader doesn't attempt to request them where they are not present.
     */
    core::ChunkProvider* chunkProvider() const;
    void setChunkProvider(core::ChunkProvider* value);

    /**
     * Archive beginning timestamp, in milliseconds since epoch, obtained from the `chunkProvider`,
     * if present. No scrolling below this boundary should be allowed.
     */
    QVariant bottomBoundMs() const;

    /**
     * The top boundary above which no scrolling should be allowed.
     * If the topmost bucket has loaded objects and extends past live, the top boundary is
     * the topmost bucket's end time, otherwise it's the live time.
     */
    qint64 topBoundMs() const;

    /**
     * Current time window, as start in milliseconds since epoch and duration in milliseconds.
     * Changing the window doesn't reset the data model structure, but scrolls it.
     */
    std::chrono::milliseconds startTime() const;
    qint64 startTimeMs() const;
    void setStartTime(std::chrono::milliseconds value);
    void setStartTimeMs(qint64 value);
    std::chrono::milliseconds duration() const;
    qint64 durationMs() const;
    void setDuration(std::chrono::milliseconds value);
    void setDurationMs(qint64 value);

    /**
     * Current live time value.
     * Set externally to ensure various things are properly synchronized.
     */
    qint64 liveTimeMs() const;
    void setLiveTimeMs(qint64 value);

    /**
     * Temporal bucket size, in milliseconds. A timeline object list resolution.
     * Changing the bucket size resets the data model structure and discards the bucket cache.
     */
    std::chrono::milliseconds bucketSize() const;
    qint64 bucketSizeMs() const;
    void setBucketSize(std::chrono::milliseconds value);
    void setBucketSizeMs(qint64 value);

    /**
     * Number of extra buckets loaded on each side of the current time window.
     * Default 2.
     */
    int preloadedBuckets() const;
    void setPreloadedBuckets(int value);

    /**
     * Maximum number of buckets kept in memory, including the ones intersecting with the window.
     * Note that the buckets intersecting with the window and the extra preloaded ones are always
     * kept in memory no matter the cache size.
     * Default 100.
     */
    int cacheSize() const;
    void setCacheSize(int value);

    /**
     * A number of seconds after which the loaded buckets are subject to reload.
     * If zero, loaded buckets are never reloaded.
     * Default 30.
     */
    std::chrono::seconds expirationTime() const;
    void setExpirationTime(std::chrono::seconds value);
    int expirationSeconds() const;
    void setExpirationSeconds(int value);

    /**
     * Maximum number of motions, bookmarks or analytics tracks to count and display in one bucket.
     * Default 100.
     */
    int maxObjectsPerBucket() const;
    void setMaxObjectsPerBucket(int value);

    /**
     * The minimum time spread of objects in one bucket in milliseconds, below which the bucket
     * is considered not expandable (the user cannot further zoom in by clicking on the tile
     * in the timeline).
     * The data representation in such bucket is slightly different from buckets with multiple
     * objects with a bigger time spread.
     * Default 500.
     */
    std::chrono::milliseconds minimumStackDuration() const;
    qint64 minimumStackDurationMs() const;
    void setMinimumStackDuration(std::chrono::milliseconds value);
    void setMinimumStackDurationMs(qint64 value);

    /**
     * Virtual chunks constructed from the loaded objects in *visible buckets* for those
     * object types that don't have native chunks (currently that is only bookmarks).
     * If a bucket has more objects than `maxObjectsPerBucket`, then the whole bucket time period
     * is included, otherwise all particular object time periods are included.
     */
    QnTimePeriodList objectChunks() const;

    /**
     * Data access methods.
     * Return *visible buckets* - buckets intersecting with the current time window.
     */
    Q_INVOKABLE int count() const;
    Q_INVOKABLE ObjectBucket bucket(int index) const;

    static void registerQmlTypes();

signals:
    void objectsTypeChanged();
    void chunkProviderChanged();
    void bottomBoundChanged();
    void topBoundChanged();
    void startTimeChanged();
    void durationChanged();
    void liveTimeChanged();
    void bucketSizeChanged();
    void preloadedBucketsChanged();
    void cacheSizeChanged();
    void maxObjectsPerBucketChanged();
    void expirationTimeChanged();
    void minimumStackDurationChanged();
    void objectChunksChanged();

    /**
     * Structure change notifications.
     * Sent when the subset of *visible buckets* is changed.
     */
    void bucketsReset();
    void scrolledOut(int first, int count);
    void scrolledIn(int first, int count);

    /**
     * Data change notification.
     * Sent when the content of one of *visible buckets* is changed.
     */
    void bucketChanged(int index);

private:
    struct InternalBucket;
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
