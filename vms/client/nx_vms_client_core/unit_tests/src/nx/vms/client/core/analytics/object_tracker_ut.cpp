// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <list>

#include <gtest/gtest.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/analytics/object_tracker.h>

namespace nx::vms::client::core::analytics {
namespace test {

using namespace std::chrono;
using namespace nx::common::metadata;

namespace {

const nx::Uuid kEngineId = nx::Uuid::createUuid();
const QRectF kRectangle(0.1, 0.1, 0.2, 0.2);

} // namespace

class ObjectTrackerTest: public ::testing::Test
{
protected:
    struct ObjectData
    {
        nx::Uuid trackId;
        QRectF boundingBox = kRectangle;
        std::string typeId = "nx.base.Person";
    };

    void givenPacket(milliseconds timestamp,
        std::vector<ObjectData> objects,
        milliseconds duration = 0ms,
        const nx::Uuid& engineId = kEngineId)
    {
        auto packet = std::make_shared<ObjectMetadataPacket>();
        packet->timestampUs = microseconds(timestamp).count();
        packet->durationUs = microseconds(duration).count();
        packet->analyticsEngineId = engineId;

        for (const auto& data: objects)
        {
            ObjectMetadata object;
            object.trackId = data.trackId;
            object.boundingBox = data.boundingBox;
            object.typeId = data.typeId;
            packet->objectMetadataList.push_back(object);
        }

        const auto position = std::upper_bound(packets.begin(),
            packets.end(),
            packet,
            [](const auto& left, const auto& right)
            { return left->timestampUs < right->timestampUs; });
        packets.insert(position, packet);
    }

    /** Feeds the tracker the packets a metadata provider would return for this position. */
    void givenPositionIs(milliseconds position) { whenPositionIs(position); }

    void whenPositionIs(milliseconds position)
    {
        std::list<ObjectMetadataPacketPtr> window;
        for (const auto& packet: packets)
        {
            const microseconds packetTimestamp(packet->timestampUs);
            if (packetTimestamp >= position - ObjectTracker::kMetadataWindowSize
                && packetTimestamp <= position + ObjectTracker::kMetadataWindowSize)
            {
                window.push_back(packet);
            }
        }

        lastTimestamp = position;
        lastResult = tracker.update(window, position);
    }

    void whenTrackerIsCleared() { tracker.clear(); }

    void thenObjectIsVisible(const nx::Uuid& trackId) const
    {
        EXPECT_TRUE(tracker.objects().contains(trackId));
    }

    void thenObjectIsNotVisible(const nx::Uuid& trackId) const
    {
        EXPECT_FALSE(tracker.objects().contains(trackId));
    }

    void thenObjectIsReportedUpdated(const nx::Uuid& trackId) const
    {
        EXPECT_TRUE(contains(lastResult.updated, trackId));
    }

    void thenObjectIsReportedRemoved(const nx::Uuid& trackId) const
    {
        EXPECT_TRUE(contains(lastResult.removed, trackId));
    }

    void thenNothingIsReportedRemoved() const { EXPECT_TRUE(lastResult.removed.empty()); }

    void thenRectangleIs(const nx::Uuid& trackId, const QRectF& expected) const
    {
        const auto actual = object(trackId).interpolatedRectangle(lastTimestamp);
        EXPECT_NEAR(expected.x(), actual.x(), kPrecision);
        EXPECT_NEAR(expected.y(), actual.y(), kPrecision);
        EXPECT_NEAR(expected.width(), actual.width(), kPrecision);
        EXPECT_NEAR(expected.height(), actual.height(), kPrecision);
    }

    void thenEndTimestampIs(const nx::Uuid& trackId, milliseconds expected) const
    {
        EXPECT_EQ(expected, object(trackId).endTimestamp);
    }

    void thenRectangleIsInvalid(const nx::Uuid& trackId) const
    {
        const auto actual = object(trackId).interpolatedRectangle(lastTimestamp);
        EXPECT_TRUE(actual.width() < 0 || actual.height() < 0)
            << actual.width() << "x" << actual.height();
    }

    const ObjectSnapshot& object(const nx::Uuid& trackId) const
    {
        const auto it = tracker.objects().constFind(trackId);
        NX_ASSERT(it != tracker.objects().cend());
        return *it;
    }

private:
    static bool contains(const std::vector<nx::Uuid>& ids, const nx::Uuid& id)
    {
        return std::find(ids.cbegin(), ids.cend(), id) != ids.cend();
    }

protected:
    static constexpr qreal kPrecision = 1e-9;

    const nx::Uuid trackA = nx::Uuid::createUuid();
    const nx::Uuid trackB = nx::Uuid::createUuid();

    std::list<ObjectMetadataPacketPtr> packets;
    ObjectTracker tracker;
    ObjectTracker::UpdateResult lastResult;
    milliseconds lastTimestamp{};
};

TEST_F(ObjectTrackerTest, objectIsNotVisibleBeforeItsFirstPacket)
{
    givenPacket(10000ms, {{trackA}});

    whenPositionIs(9999ms);

    thenObjectIsNotVisible(trackA);
}

TEST_F(ObjectTrackerTest, objectAppearsAtItsPacketTimestamp)
{
    givenPacket(10000ms, {{trackA}});

    whenPositionIs(10000ms);

    thenObjectIsVisible(trackA);
    thenObjectIsReportedUpdated(trackA);
    EXPECT_EQ(kEngineId, object(trackA).analyticsEngineId);
    EXPECT_EQ(10000ms, object(trackA).startTimestamp);
}

TEST_F(ObjectTrackerTest, objectWithoutDurationLivesForMinimalDurationAfterLastPacket)
{
    givenPacket(10000ms, {{trackA}});

    whenPositionIs(10100ms);
    thenObjectIsVisible(trackA);
    EXPECT_EQ(10000ms + kMinimalObjectDuration, object(trackA).endTimestamp);

    whenPositionIs(10201ms);
    thenObjectIsNotVisible(trackA);
    thenObjectIsReportedRemoved(trackA);
}

TEST_F(ObjectTrackerTest, packetDurationProlongsObjectLifetime)
{
    givenPacket(10000ms, {{trackA}}, /*duration*/ 1s);

    whenPositionIs(10900ms);
    thenObjectIsVisible(trackA);
    EXPECT_EQ(11000ms, object(trackA).endTimestamp);

    whenPositionIs(11001ms);
    thenObjectIsNotVisible(trackA);
    thenObjectIsReportedRemoved(trackA);
}

TEST_F(ObjectTrackerTest, objectIsVisibleJustBeforeItsEnd)
{
    givenPacket(10000ms, {{trackA}});

    whenPositionIs(10000ms + kMinimalObjectDuration - 1ms);

    thenObjectIsVisible(trackA);
}

TEST_F(ObjectTrackerTest, shownObjectIsRemovedAtItsEnd)
{
    givenPacket(10000ms, {{trackA}});
    givenPositionIs(10100ms);

    whenPositionIs(10000ms + kMinimalObjectDuration);

    thenObjectIsNotVisible(trackA);
    thenObjectIsReportedRemoved(trackA);
}

TEST_F(ObjectTrackerTest, objectFirstSeenAtItsEndIsNotVisible)
{
    givenPacket(10000ms, {{trackA}});

    whenPositionIs(10000ms + kMinimalObjectDuration);

    thenObjectIsNotVisible(trackA);
}

TEST_F(ObjectTrackerTest, latestPacketDurationIsUsed)
{
    givenPacket(10000ms, {{trackA}}, /*duration*/ 1s);
    givenPacket(10500ms, {{trackA}}, /*duration*/ 2s);

    whenPositionIs(10600ms);

    thenEndTimestampIs(trackA, 12500ms);
}

TEST_F(ObjectTrackerTest, packetDurationIsCountedFromCurrentPacket)
{
    givenPacket(10000ms, {{trackA}}, /*duration*/ 1s);
    givenPacket(10500ms, {{trackA}});

    whenPositionIs(10600ms);

    thenEndTimestampIs(trackA, 11500ms);
}

TEST_F(ObjectTrackerTest, objectStaysVisibleBetweenDistantPacketsOfItsTrack)
{
    givenPacket(10000ms, {{trackA}});
    givenPacket(11000ms, {{trackA}});

    whenPositionIs(10500ms);

    thenObjectIsVisible(trackA);
    EXPECT_EQ(10000ms, object(trackA).startTimestamp);
    EXPECT_EQ(11000ms + kMinimalObjectDuration, object(trackA).endTimestamp);
}

TEST_F(ObjectTrackerTest, objectDataComesFromLatestPacketBeforePosition)
{
    givenPacket(10000ms, {{.trackId = trackA, .typeId = "nx.base.Person"}});
    givenPacket(10100ms, {{.trackId = trackA, .typeId = "nx.base.Face"}});
    givenPacket(10200ms, {{.trackId = trackA, .typeId = "nx.base.Car"}});

    whenPositionIs(10150ms);

    EXPECT_EQ("nx.base.Face", object(trackA).metadata.typeId);
    EXPECT_EQ(10100ms, object(trackA).startTimestamp);
    EXPECT_EQ(10200ms, object(trackA).futureRectangleTimestamp);
}

TEST_F(ObjectTrackerTest, rectangleIsInterpolatedBetweenPackets)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = QRectF(0.0, 0.0, 0.2, 0.2)}});
    givenPacket(11000ms, {{.trackId = trackA, .boundingBox = QRectF(0.4, 0.2, 0.4, 0.2)}});

    whenPositionIs(10250ms);

    thenRectangleIs(trackA, QRectF(0.1, 0.05, 0.25, 0.2));
}

TEST_F(ObjectTrackerTest, rectangleIsNotMovedWhenThereIsNoNextPacket)
{
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = kRectangle}});

    whenPositionIs(10150ms);

    thenRectangleIs(trackA, kRectangle);
}

TEST_F(ObjectTrackerTest, nextPacketRectangleIsUsedWhenCurrentRectangleIsInvalid)
{
    const QRectF kInvalidRectangle(0.5, 0.5, -0.1, -0.1);
    const QRectF kNextRectangle(0.3, 0.3, 0.1, 0.1);
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = kInvalidRectangle}});
    givenPacket(10100ms, {{.trackId = trackA, .boundingBox = kNextRectangle}});

    whenPositionIs(10050ms);

    thenRectangleIs(trackA, kNextRectangle);
}

TEST_F(ObjectTrackerTest, rectangleIsNotMovedTowardsInvalidNextRectangle)
{
    const QRectF kInvalidRectangle(0.5, 0.5, -0.1, -0.1);
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = kRectangle}});
    givenPacket(10100ms, {{.trackId = trackA, .boundingBox = kInvalidRectangle}});

    whenPositionIs(10050ms);

    thenRectangleIs(trackA, kRectangle);
}

TEST_F(ObjectTrackerTest, rectangleStaysInvalidWhenItIsInvalidAndThereIsNoNextPacket)
{
    const QRectF kInvalidRectangle(0.5, 0.5, -0.1, -0.1);
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = kInvalidRectangle}});

    whenPositionIs(10050ms);

    thenRectangleIsInvalid(trackA);
}

TEST_F(ObjectTrackerTest, rectangleOfPacketSkippedByPlaybackIsNotUsed)
{
    const QRectF kInvalidRectangle(0.5, 0.5, -0.1, -0.1);
    givenPacket(10000ms, {{.trackId = trackA, .boundingBox = kRectangle}});
    givenPacket(10100ms, {{.trackId = trackA, .boundingBox = kRectangle}});
    givenPacket(10200ms, {{.trackId = trackA, .boundingBox = kInvalidRectangle}});
    givenPositionIs(10000ms);

    whenPositionIs(10250ms);

    thenRectangleIsInvalid(trackA);
}

TEST_F(ObjectTrackerTest, tracksAreIndependent)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    givenPacket(10150ms, {{trackB}});

    whenPositionIs(10150ms);
    thenObjectIsVisible(trackA);
    thenObjectIsVisible(trackB);

    whenPositionIs(10300ms);
    thenObjectIsNotVisible(trackA);
    thenObjectIsReportedRemoved(trackA);
    thenObjectIsVisible(trackB);
}

TEST_F(ObjectTrackerTest, visibleObjectIsNotReportedRemoved)
{
    givenPacket(10000ms, {{trackA}});
    givenPacket(10100ms, {{trackA}});

    whenPositionIs(10000ms);
    whenPositionIs(10100ms);

    thenObjectIsVisible(trackA);
    thenNothingIsReportedRemoved();
}

TEST_F(ObjectTrackerTest, seekingBeforeObjectStartRemovesIt)
{
    givenPacket(10000ms, {{trackA}});
    whenPositionIs(10100ms);

    whenPositionIs(5000ms);

    thenObjectIsNotVisible(trackA);
    thenObjectIsReportedRemoved(trackA);
}

TEST_F(ObjectTrackerTest, clearingRemovesAllObjects)
{
    givenPacket(10000ms, {{trackA}, {trackB}});
    whenPositionIs(10000ms);

    whenTrackerIsCleared();

    EXPECT_TRUE(tracker.objects().isEmpty());
}

} // namespace test
} // namespace nx::vms::client::core::analytics
