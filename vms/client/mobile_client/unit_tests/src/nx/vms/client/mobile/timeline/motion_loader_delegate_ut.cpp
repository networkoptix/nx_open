// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/media/time_period_storage.h>
#include <nx/vms/client/mobile/timeline/motion_data_helper.h>
#include <nx/vms/client/mobile/timeline/motion_loader_delegate.h>

#include "../test_support/test_context.h"

namespace nx::vms::client::mobile::timeline::test {

using namespace std::chrono;

class MotionLoaderDelegateTest: public mobile::test::ApplicationContextBasedTest
{
protected:
    static constexpr int kLimit = 3;
    static constexpr QnTimePeriod kPeriod{100000, 100000};

    virtual void SetUp() override
    {
        m_camera = addCamera();
        m_storage = std::make_unique<core::TimePeriodStorage>();
    }

    virtual void TearDown() override
    {
        m_delegate.reset();
        m_storage.reset();
        m_camera.reset();
        mobile::test::ApplicationContextBasedTest::TearDown();
    }

    void givenChunks(const std::vector<QnTimePeriod>& chunks)
    {
        QnTimePeriodList periods;
        for (const auto& chunk: chunks)
            periods.push_back(chunk);

        m_storage->setPeriods(Qn::MotionContent, periods);
    }

    void givenNoStorage() { m_storage.reset(); }
    void givenNoResource() { m_withResource = false; }

    void whenStorageDestroyed()
    {
        ensureDelegate();
        m_storage.reset();
    }

    void whenLoaded(const QnTimePeriod& period = kPeriod)
    {
        ensureDelegate();
        m_future = m_delegate->load(period, /*minimumStackDuration*/ 1min);
    }

    void thenNothingIsLoaded() { ASSERT_TRUE(m_future.isCanceled()); }

    /** Motion chunks are already loaded, so the result is ready immediately. */
    void thenResultIsReadyImmediately()
    {
        ASSERT_TRUE(m_future.isFinished());
        ASSERT_EQ(m_future.resultCount(), 1);
        m_result = m_future.result();
    }

    void andObjectCountIs(int count) { ASSERT_EQ(m_result.count, count); }
    void andPositionIs(qint64 positionMs) { ASSERT_EQ(m_result.positionMs, positionMs); }
    void andDurationIs(qint64 durationMs) { ASSERT_EQ(m_result.durationMs, durationMs); }
    void andCaptionIs(const QString& caption) { ASSERT_EQ(m_result.caption, caption); }

    void andSingleObjectIs(const QnTimePeriod& chunk)
    {
        ASSERT_EQ(m_result.perObjectData.size(), 1);
        ASSERT_EQ(m_result.perObjectData[0].source.value<QnTimePeriod>(), chunk);
    }

private:
    void ensureDelegate()
    {
        if (m_delegate)
            return;

        m_delegate = std::make_unique<MotionLoaderDelegate>(
            m_storage.get(), m_withResource ? QnResourcePtr(m_camera) : QnResourcePtr(), kLimit);
    }

private:
    QnVirtualCameraResourcePtr m_camera;
    std::unique_ptr<core::TimePeriodStorage> m_storage;
    bool m_withResource = true;

    std::unique_ptr<MotionLoaderDelegate> m_delegate;
    QFuture<MultiObjectData> m_future;
    MultiObjectData m_result;
};

TEST_F(MotionLoaderDelegateTest, emptyPeriod)
{
    givenChunks({{kPeriod.startTimeMs, 1000}});
    whenLoaded(QnTimePeriod());
    thenNothingIsLoaded();
}

TEST_F(MotionLoaderDelegateTest, noStorage)
{
    givenNoStorage();
    whenLoaded();
    thenNothingIsLoaded();
}

TEST_F(MotionLoaderDelegateTest, storageDestroyed)
{
    givenChunks({{kPeriod.startTimeMs, 1000}});
    whenStorageDestroyed();
    whenLoaded();
    thenNothingIsLoaded();
}

TEST_F(MotionLoaderDelegateTest, noResource)
{
    givenChunks({{kPeriod.startTimeMs, 1000}});
    givenNoResource();
    whenLoaded();
    thenNothingIsLoaded();
}

TEST_F(MotionLoaderDelegateTest, noChunks)
{
    givenChunks({});
    whenLoaded();
    thenNothingIsLoaded();
}

TEST_F(MotionLoaderDelegateTest, noChunksStartingWithinPeriod)
{
    givenChunks({{kPeriod.startTimeMs - 10000, 20000}, {kPeriod.endTimeMs(), 1000}});
    whenLoaded();
    thenNothingIsLoaded();
}

TEST_F(MotionLoaderDelegateTest, singleChunk)
{
    givenChunks({{kPeriod.startTimeMs + 1000, 2000}});
    whenLoaded();
    thenResultIsReadyImmediately();
    andObjectCountIs(1);
    andPositionIs(kPeriod.startTimeMs + 1000);
    andDurationIs(2000);
    andSingleObjectIs({kPeriod.startTimeMs + 1000, 2000});
}

TEST_F(MotionLoaderDelegateTest, chunksAtPeriodBoundaries)
{
    // Only the chunks starting within the period are loaded.
    givenChunks({{kPeriod.startTimeMs - 1, 1000},
        {kPeriod.startTimeMs, 1000},
        {kPeriod.endTimeMs() - 1, 1000},
        {kPeriod.endTimeMs(), 1000}});
    whenLoaded();
    thenResultIsReadyImmediately();
    andObjectCountIs(2);
    andPositionIs(kPeriod.startTimeMs);
    andDurationIs(kPeriod.durationMs - 1);
    andCaptionIs(MotionDataHelper::tr("Motion (%1)").arg(2));
}

TEST_F(MotionLoaderDelegateTest, chunksOverLimit)
{
    std::vector<QnTimePeriod> chunks;
    for (int i = 0; i < kLimit + 2; ++i)
        chunks.push_back({kPeriod.startTimeMs + i * 1000, 500});

    givenChunks(chunks);
    whenLoaded();
    thenResultIsReadyImmediately();
    andObjectCountIs(kLimit + 2);
    andCaptionIs(MotionDataHelper::tr("Motion (>%1)").arg(kLimit));
}

} // namespace nx::vms::client::mobile::timeline::test
