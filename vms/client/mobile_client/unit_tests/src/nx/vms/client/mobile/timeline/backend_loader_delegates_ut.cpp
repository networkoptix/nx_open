// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>

#include <analytics/db/analytics_db_types.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
// Required on Windows to import `AbstractBackend` instantiations exported from the client core.
#include <nx/vms/client/core/timeline/backends/analytics_backend.h>
#include <nx/vms/client/core/timeline/backends/bookmarks_backend.h>
#include <nx/vms/client/mobile/timeline/analytics_data_helper.h>
#include <nx/vms/client/mobile/timeline/analytics_loader_delegate.h>
#include <nx/vms/client/mobile/timeline/bookmark_data_helper.h>
#include <nx/vms/client/mobile/timeline/bookmark_loader_delegate.h>

#include "../test_support/test_context.h"
#include "test_backend.h"

namespace nx::vms::client::mobile::timeline::test {

using namespace std::chrono;

namespace {

struct BookmarkTraits
{
    using DataList = QnCameraBookmarkList;
    using Delegate = BookmarkLoaderDelegate;
    using Source = common::CameraBookmark;

    static common::CameraBookmark makeItem(const QnResourcePtr& camera, milliseconds timestamp)
    {
        common::CameraBookmark bookmark;
        bookmark.guid = nx::Uuid::createUuid();
        bookmark.cameraId = camera->getId();
        bookmark.startTimeMs = timestamp;
        bookmark.durationMs = 1s;
        return bookmark;
    }

    static nx::Uuid id(const common::CameraBookmark& bookmark) { return bookmark.guid; }

    static QString caption(int count)
    {
        return BookmarkDataHelper::tr("Bookmarks (%1)").arg(count);
    }

    static QString captionOverLimit(int limit)
    {
        return BookmarkDataHelper::tr("Bookmarks (>%1)").arg(limit);
    }
};

struct AnalyticsTraits
{
    using DataList = nx::analytics::db::LookupResult;
    using Delegate = AnalyticsLoaderDelegate;
    using Source = nx::analytics::db::ObjectTrack;

    static nx::analytics::db::ObjectTrackEx makeItem(
        const QnResourcePtr& camera, milliseconds timestamp)
    {
        nx::analytics::db::ObjectTrackEx track;
        track.id = nx::Uuid::createUuid();
        track.deviceId = camera->getId();
        track.firstAppearanceTimeUs = microseconds(timestamp).count();
        track.lastAppearanceTimeUs = microseconds(timestamp + 1s).count();
        return track;
    }

    static nx::Uuid id(const nx::analytics::db::ObjectTrack& track) { return track.id; }

    static QString caption(int count)
    {
        return AnalyticsDataHelper::tr("Objects (%1)").arg(count);
    }

    static QString captionOverLimit(int limit)
    {
        return AnalyticsDataHelper::tr("Objects (>%1)").arg(limit);
    }
};

} // namespace

template<typename Traits>
class BackendLoaderDelegateTest: public mobile::test::ApplicationContextBasedTest
{
protected:
    using DataList = typename Traits::DataList;
    using Backend = TestBackend<DataList>;
    using Mode = typename Backend::Mode;

    static constexpr int kLimit = 3;
    static constexpr milliseconds kMinimumStackDuration = 1min;
    static inline const QnTimePeriod kPeriod{100000ms, 100000ms};

    virtual void SetUp() override { m_camera = addCamera(); }

    virtual void TearDown() override
    {
        m_delegate.reset();
        m_backend.reset();
        m_camera.reset();
        mobile::test::ApplicationContextBasedTest::TearDown();
    }

    /** Creates objects within `kPeriod`, starting 10 seconds after its start. */
    void givenObjects(int count, milliseconds spacing = 1s)
    {
        m_data = {};
        for (int i = count - 1; i >= 0; --i) //< Sorted by timestamp in descending order.
        {
            m_data.push_back(
                Traits::makeItem(m_camera, milliseconds(kPeriod.startTimeMs) + 10s + i * spacing));
        }
    }

    void givenDeferredBackend() { m_mode = Mode::deferred; }
    void givenCanceledBackend() { m_mode = Mode::canceled; }
    void givenBackendWithoutResource() { m_withResource = false; }
    void givenLimit(int value) { m_limit = value; }

    void whenLoaded(const QnTimePeriod& period = kPeriod)
    {
        m_backend = std::make_shared<Backend>(
            m_withResource ? QnResourcePtr(m_camera) : QnResourcePtr(), m_data, m_mode);
        m_delegate = std::make_unique<typename Traits::Delegate>(m_backend, m_limit);
        m_future = m_delegate->load(period, kMinimumStackDuration);
    }

    void whenBackendFinished() { m_backend->finish(); }

    void whenBackendFinishedInAnotherThread()
    {
        std::thread([this]() { m_backend->finish(); }).join();
    }

    void whenChainCanceled() { m_future.cancelChain(); }
    void whenDelegateDestroyed() { m_delegate.reset(); }

    /** The delegate returns a canceled future without sending a request. */
    void thenLoadingIsRejected()
    {
        ASSERT_TRUE(m_future.isCanceled());
        ASSERT_TRUE(m_backend->requests.empty());
    }

    void thenLoadingIsCanceled()
    {
        ASSERT_TRUE(m_future.isCanceled());
        ASSERT_EQ(m_future.resultCount(), 0);
    }

    void thenOneMoreThanLimitIsRequested()
    {
        ASSERT_EQ(m_backend->requests.size(), 1);
        ASSERT_EQ(m_backend->requests[0].period, kPeriod);
        ASSERT_EQ(m_backend->requests[0].limit, m_limit + 1);
    }

    void thenLoadingIsNotFinished()
    {
        QCoreApplication::processEvents();
        ASSERT_FALSE(m_future.isFinished());
    }

    /** The result is merged in the system context thread, so it's not delivered immediately. */
    void thenResultIsPending() { ASSERT_FALSE(m_future.isFinished()); }

    void thenBackendRequestIsCanceled() { ASSERT_TRUE(m_backend->isPendingCanceled()); }

    void thenThereIsNoResult()
    {
        ASSERT_TRUE(waitForCompletion(m_future));
        ASSERT_EQ(m_future.resultCount(), 0);
    }

    void thenResultIsReady()
    {
        ASSERT_TRUE(waitForCompletion(m_future));
        ASSERT_EQ(m_future.resultCount(), 1);
        m_result = m_future.result();
    }

    void andObjectCountIs(int count) { ASSERT_EQ(m_result.count, count); }
    void andPositionIs(qint64 positionMs) { ASSERT_EQ(m_result.positionMs, positionMs); }
    void andDurationIs(qint64 durationMs) { ASSERT_EQ(m_result.durationMs, durationMs); }
    void andCaptionIs(const QString& caption) { ASSERT_EQ(m_result.caption, caption); }
    void andPreviewImageCountIs(int count) { ASSERT_EQ(m_result.imagePaths.size(), count); }
    void andObjectsAreNotStacked() { ASSERT_TRUE(m_result.perObjectData.isEmpty()); }

    /** The per-object data is made of the given objects, in ascending order of timestamps. */
    void andObjectsAreStacked()
    {
        const auto& objects = m_result.perObjectData;
        ASSERT_EQ(objects.size(), (qsizetype) m_data.size());

        for (int i = 0; i < (int) objects.size(); ++i)
        {
            const auto& object = objects[i];
            ASSERT_EQ(object.source.metaType(), QMetaType::fromType<typename Traits::Source>());
            ASSERT_EQ(object.id, Traits::id(m_data[m_data.size() - 1 - i]));
            if (i > 0)
                ASSERT_GT(object.startTimeMs, objects[i - 1].startTimeMs);
        }
    }

private:
    QnVirtualCameraResourcePtr m_camera;
    DataList m_data;
    Mode m_mode = Mode::ready;
    bool m_withResource = true;
    int m_limit = kLimit;

    std::shared_ptr<Backend> m_backend;
    std::unique_ptr<AbstractLoaderDelegate> m_delegate;
    QFuture<MultiObjectData> m_future;
    MultiObjectData m_result;
};

using DelegateTypes = ::testing::Types<BookmarkTraits, AnalyticsTraits>;
TYPED_TEST_SUITE(BackendLoaderDelegateTest, DelegateTypes);

TYPED_TEST(BackendLoaderDelegateTest, emptyPeriod)
{
    this->givenObjects(1);
    this->whenLoaded(QnTimePeriod());
    this->thenLoadingIsRejected();
}

TYPED_TEST(BackendLoaderDelegateTest, requestsOneMoreThanLimit)
{
    this->givenObjects(1);
    this->whenLoaded();
    this->thenOneMoreThanLimitIsRequested();
}

TYPED_TEST(BackendLoaderDelegateTest, canceledBackendFuture)
{
    this->givenObjects(1);
    this->givenCanceledBackend();
    this->whenLoaded();
    this->thenLoadingIsCanceled();
}

TYPED_TEST(BackendLoaderDelegateTest, singleObject)
{
    this->givenObjects(1);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andObjectCountIs(1);
    this->andPositionIs(this->kPeriod.startTimeMs + 10000);
    this->andObjectsAreStacked();
}

TYPED_TEST(BackendLoaderDelegateTest, multipleObjectsWithinLimit)
{
    this->givenObjects(this->kLimit);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andObjectCountIs(this->kLimit);
    this->andPositionIs(this->kPeriod.startTimeMs + 10000);
    this->andObjectsAreStacked();
}

TYPED_TEST(BackendLoaderDelegateTest, multipleObjectsOverLimit)
{
    // The result is cropped, so it starts at the period start, and ends at the latest object,
    // which is 10 seconds after the period start plus 1 second for each next object.
    this->givenObjects(this->kLimit + 5);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andObjectCountIs(this->kLimit + 1);
    this->andPositionIs(this->kPeriod.startTimeMs);
    this->andDurationIs(10000 + (this->kLimit + 4) * 1000);
}

TYPED_TEST(BackendLoaderDelegateTest, deferredResult)
{
    this->givenObjects(2);
    this->givenDeferredBackend();
    this->whenLoaded();
    this->thenLoadingIsNotFinished();

    this->whenBackendFinished();
    this->thenResultIsReady();
    this->andObjectCountIs(2);
}

TYPED_TEST(BackendLoaderDelegateTest, noResource)
{
    this->givenObjects(1);
    this->givenBackendWithoutResource();
    this->whenLoaded();
    this->thenLoadingIsRejected();
}

TYPED_TEST(BackendLoaderDelegateTest, cancelChain)
{
    this->givenObjects(1);
    this->givenDeferredBackend();
    this->whenLoaded();
    this->whenChainCanceled();
    this->thenBackendRequestIsCanceled();

    this->whenBackendFinished();
    this->thenThereIsNoResult();
}

TYPED_TEST(BackendLoaderDelegateTest, loadOutlivesDelegate)
{
    this->givenObjects(2);
    this->givenDeferredBackend();
    this->whenLoaded();
    this->whenDelegateDestroyed();
    this->thenLoadingIsNotFinished();

    this->whenBackendFinished();
    this->thenResultIsReady();
    this->andObjectCountIs(2);
}

TYPED_TEST(BackendLoaderDelegateTest, resultFromAnotherThread)
{
    this->givenObjects(this->kLimit);
    this->givenDeferredBackend();
    this->whenLoaded();
    this->whenBackendFinishedInAnotherThread();
    this->thenResultIsPending();
    this->thenResultIsReady();
    this->andObjectCountIs(this->kLimit);
    this->andObjectsAreStacked();
}

TYPED_TEST(BackendLoaderDelegateTest, multipleObjectsNotStacked)
{
    // The objects span more than the minimum stack duration.
    this->givenObjects(2, /*spacing*/ 70s);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andObjectCountIs(2);
    this->andObjectsAreNotStacked();
}

TYPED_TEST(BackendLoaderDelegateTest, captionWithinLimit)
{
    this->givenObjects(this->kLimit);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andCaptionIs(TypeParam::caption(this->kLimit));
}

TYPED_TEST(BackendLoaderDelegateTest, captionOverLimit)
{
    this->givenObjects(this->kLimit + 5);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andCaptionIs(TypeParam::captionOverLimit(this->kLimit));
}

TYPED_TEST(BackendLoaderDelegateTest, previewImagesAreLimited)
{
    this->givenLimit(10);
    this->givenObjects(kMaxPreviewImageCount + 2);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andObjectCountIs(kMaxPreviewImageCount + 2);
    this->andPreviewImageCountIs(kMaxPreviewImageCount);
}

TYPED_TEST(BackendLoaderDelegateTest, noObjects)
{
    this->givenObjects(0);
    this->whenLoaded();
    this->thenResultIsReady();
    this->andObjectCountIs(0);
    this->andObjectsAreNotStacked();
}

} // namespace nx::vms::client::mobile::timeline::test
