// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>
#include <QtTest/QSignalSpy>

#include <analytics/common/object_metadata.h>
#include <core/resource/resource.h>
#include <nx/analytics/abstract_metadata_consumer.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/vms/client/core/analytics/media_player_analytics_provider.h>
#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/core/media/media_player.h>

namespace nx::vms::client::core::test {

namespace {

using namespace std::chrono;
using namespace nx::common::metadata;

std::vector<milliseconds> packetTimestamps(
    const AbstractAnalyticsMetadataProviderPtr& metadataProvider, int channel)
{
    std::vector<milliseconds> result;
    if (!metadataProvider)
        return result;

    for (const auto& packet: metadataProvider->metadataRange(0us, microseconds::max(), channel))
        result.push_back(duration_cast<milliseconds>(microseconds(packet->timestampUs)));
    return result;
}

} // namespace

class TestMediaPlayerAnalyticsProvider: public MediaPlayerAnalyticsProvider
{
public:
    using MediaPlayerAnalyticsProvider::metadataConsumer;
};

class MediaPlayerAnalyticsProviderTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        provider = std::make_unique<TestMediaPlayerAnalyticsProvider>();
        metadataProviderChangedSpy = std::make_unique<QSignalSpy>(
            provider.get(), &MediaPlayerAnalyticsProvider::metadataProviderChanged);
        mediaPlayerChangedSpy = std::make_unique<QSignalSpy>(
            provider.get(), &MediaPlayerAnalyticsProvider::mediaPlayerChanged);
        metadataReceivedSpy = std::make_unique<QSignalSpy>(
            provider.get(), &MediaPlayerAnalyticsProvider::metadataReceived);

        QObject::connect(provider.get(),
            &MediaPlayerAnalyticsProvider::metadataReceived,
            provider.get(),
            [this]()
            {
                packetsProvidedWhenSignaled =
                    packetTimestamps(provider->metadataProvider(), /*channel*/ 0);
            });
    }

    virtual void TearDown() override
    {
        provider.reset();
        player.reset();
        otherPlayer.reset();
    }

    void givenMediaPlayer() { whenMediaPlayerSet(); }

    void givenActiveProvider() { whenActivated(); }

    void givenDeactivatedProvider() { whenDeactivated(); }

    void givenObjectsReceived(milliseconds timestamp, int channel = 0)
    {
        whenObjectsReceived(timestamp, channel);

        const auto timestamps = packetTimestamps(provider->metadataProvider(), channel);
        ASSERT_TRUE(std::ranges::find(timestamps, timestamp) != timestamps.end());
    }

    void givenResourceChanged() { whenResourceChanged(); }

    void givenMediaPlayerSwitched() { whenAnotherMediaPlayerSet(); }

    void givenMetadataProviderRemembered()
    {
        rememberedMetadataProvider = provider->metadataProvider();
        ASSERT_TRUE(rememberedMetadataProvider);
    }

    void givenSignalsReset()
    {
        metadataProviderChangedSpy->clear();
        mediaPlayerChangedSpy->clear();
        metadataReceivedSpy->clear();
    }

    void givenObjectsReceivedAndSignaled(milliseconds timestamp)
    {
        givenObjectsReceived(timestamp);
        whenEventsAreProcessed();
    }

    void whenActivated() { provider->setActive(true); }

    void whenDeactivated() { provider->setActive(false); }

    void whenMediaPlayerSet()
    {
        player = std::make_unique<MediaPlayer>();
        provider->setMediaPlayer(player.get());
    }

    void whenAnotherMediaPlayerSet()
    {
        otherPlayer = std::make_unique<MediaPlayer>();
        provider->setMediaPlayer(otherPlayer.get());
    }

    void whenMediaPlayerReset() { provider->setMediaPlayer(nullptr); }

    void whenMediaPlayerDestroyed() { player.reset(); }

    void whenResourceChanged() { player->setResource(QnResourcePtr(new QnResource())); }

    void whenPreviousMediaPlayerResourceChanged() { whenResourceChanged(); }

    void whenProviderDestroyed() { provider.reset(); }

    void whenEventsAreProcessed() { QCoreApplication::processEvents(); }

    /** Feeds the metadata as the media player would do it with the received stream data. */
    void whenObjectsReceived(milliseconds timestamp, int channel = 0)
    {
        const auto consumer = provider->metadataConsumer();
        ASSERT_TRUE(consumer);

        ObjectMetadataPacket packet;
        packet.timestampUs = microseconds(timestamp).count();
        ObjectMetadata object;
        object.trackId = nx::Uuid::createUuid();
        object.boundingBox = QRectF(0.1, 0.1, 0.2, 0.2);
        packet.objectMetadataList.push_back(object);

        auto metadata = std::make_shared<QnCompressedMetadata>(MetadataType::ObjectDetection);
        metadata->setData(QnUbjson::serialized(packet));
        metadata->channelNumber = channel;
        metadata->setTimestampUsec(packet.timestampUs);
        consumer->processMetadata(metadata);
    }

    void thenThereIsNoMetadataProvider() const { EXPECT_FALSE(provider->metadataProvider()); }

    void thenThereIsMetadataProvider() const { EXPECT_TRUE(provider->metadataProvider()); }

    void thenProvidedPacketsAre(const std::vector<milliseconds>& timestamps, int channel = 0) const
    {
        EXPECT_EQ(timestamps, packetTimestamps(provider->metadataProvider(), channel));
    }

    void thenRememberedMetadataProviderProvides(const std::vector<milliseconds>& timestamps) const
    {
        EXPECT_EQ(timestamps, packetTimestamps(rememberedMetadataProvider, /*channel*/ 0));
    }

    void thenMetadataProviderChangedTimes(int count) const
    {
        EXPECT_EQ(count, metadataProviderChangedSpy->count());
    }

    void thenMediaPlayerIs(const MediaPlayer* expected) const
    {
        EXPECT_EQ(expected, provider->mediaPlayer());
    }

    void thenMediaPlayerChangedTimes(int count) const
    {
        EXPECT_EQ(count, mediaPlayerChangedSpy->count());
    }

    void thenMetadataReceivedTimes(int count) const
    {
        EXPECT_EQ(count, metadataReceivedSpy->count());
    }

    void thenPacketsProvidedWhenSignaledWere(const std::vector<milliseconds>& timestamps) const
    {
        EXPECT_EQ(timestamps, packetsProvidedWhenSignaled);
    }

protected:
    std::unique_ptr<MediaPlayer> player;
    std::unique_ptr<MediaPlayer> otherPlayer;
    std::unique_ptr<TestMediaPlayerAnalyticsProvider> provider;

    AbstractAnalyticsMetadataProviderPtr rememberedMetadataProvider;

    std::unique_ptr<QSignalSpy> metadataProviderChangedSpy;
    std::unique_ptr<QSignalSpy> mediaPlayerChangedSpy;
    std::unique_ptr<QSignalSpy> metadataReceivedSpy;

    std::vector<milliseconds> packetsProvidedWhenSignaled;
};

// Activation.

TEST_F(MediaPlayerAnalyticsProviderTest, metadataIsNotProvidedWhileInactive)
{
    whenMediaPlayerSet();

    thenThereIsNoMetadataProvider();
}

TEST_F(MediaPlayerAnalyticsProviderTest, metadataIsNotProvidedWithoutMediaPlayer)
{
    whenActivated();

    thenThereIsNoMetadataProvider();
    thenMetadataProviderChangedTimes(0);
}

TEST_F(MediaPlayerAnalyticsProviderTest, activationProvidesMetadata)
{
    givenMediaPlayer();
    givenSignalsReset();

    whenActivated();

    thenThereIsMetadataProvider();
    thenMetadataProviderChangedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, mediaPlayerSetWhileActiveProvidesMetadata)
{
    givenActiveProvider();

    whenMediaPlayerSet();

    thenThereIsMetadataProvider();
    thenMetadataProviderChangedTimes(1);
    thenMediaPlayerChangedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, mediaPlayerSetWhileInactiveDoesNotChangeMetadataProvider)
{
    whenMediaPlayerSet();

    thenThereIsNoMetadataProvider();
    thenMetadataProviderChangedTimes(0);
    thenMediaPlayerChangedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, deactivationStopsProvidingMetadata)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenSignalsReset();

    whenDeactivated();

    thenThereIsNoMetadataProvider();
    thenMetadataProviderChangedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, reactivationDoesNotProvidePreviousMetadata)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenDeactivatedProvider();

    whenActivated();

    thenProvidedPacketsAre({});
}

// Received metadata.

TEST_F(MediaPlayerAnalyticsProviderTest, receivedObjectsAreProvided)
{
    givenMediaPlayer();
    givenActiveProvider();

    whenObjectsReceived(10000ms);
    whenObjectsReceived(10100ms);

    thenProvidedPacketsAre({10000ms, 10100ms});
}

TEST_F(MediaPlayerAnalyticsProviderTest, receivedObjectsAreProvidedForTheirChannel)
{
    givenMediaPlayer();
    givenActiveProvider();

    whenObjectsReceived(10000ms, /*channel*/ 1);

    thenProvidedPacketsAre({}, /*channel*/ 0);
    thenProvidedPacketsAre({10000ms}, /*channel*/ 1);
}

// Notification about received metadata.

TEST_F(MediaPlayerAnalyticsProviderTest, receivedObjectsAreSignaledAfterTheyAreProvided)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);

    whenEventsAreProcessed();

    thenMetadataReceivedTimes(1);
    thenPacketsProvidedWhenSignaledWere({10000ms});
}

TEST_F(MediaPlayerAnalyticsProviderTest, receivedObjectsAreNotSignaledImmediately)
{
    givenMediaPlayer();
    givenActiveProvider();

    whenObjectsReceived(10000ms);

    thenMetadataReceivedTimes(0);
}

TEST_F(MediaPlayerAnalyticsProviderTest, objectsReceivedTogetherAreSignaledOnce)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenObjectsReceived(10100ms);
    givenObjectsReceived(10200ms, /*channel*/ 1);

    whenEventsAreProcessed();

    thenMetadataReceivedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, objectsReceivedSeparatelyAreSignaledSeparately)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceivedAndSignaled(10000ms);
    givenObjectsReceived(10100ms);

    whenEventsAreProcessed();

    thenMetadataReceivedTimes(2);
    thenPacketsProvidedWhenSignaledWere({10000ms, 10100ms});
}

TEST_F(MediaPlayerAnalyticsProviderTest, objectsOfNewResourceAreSignaled)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceivedAndSignaled(10000ms);
    givenResourceChanged();
    givenSignalsReset();
    givenObjectsReceived(20000ms);

    whenEventsAreProcessed();

    thenMetadataReceivedTimes(1);
    thenPacketsProvidedWhenSignaledWere({20000ms});
}

TEST_F(MediaPlayerAnalyticsProviderTest, providerDestroyedBeforeSignalingIsSafe)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);

    whenProviderDestroyed();
    whenEventsAreProcessed();

    // Nothing to check: there is no provider to signal, and the test must not crash.
}

// Resource and media player changes.

TEST_F(MediaPlayerAnalyticsProviderTest, resourceChangeDropsPreviousResourceMetadata)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenSignalsReset();

    whenResourceChanged();

    thenProvidedPacketsAre({});
    thenMetadataProviderChangedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, objectsOfNewResourceAreProvided)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenResourceChanged();

    whenObjectsReceived(20000ms);

    thenProvidedPacketsAre({20000ms});
}

TEST_F(MediaPlayerAnalyticsProviderTest, anotherMediaPlayerDoesNotProvidePreviousMetadata)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenSignalsReset();

    whenAnotherMediaPlayerSet();

    thenMediaPlayerIs(otherPlayer.get());
    thenProvidedPacketsAre({});
    thenMetadataProviderChangedTimes(1);
    thenMediaPlayerChangedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, previousMediaPlayerResourceChangeIsIgnored)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenMediaPlayerSwitched();
    givenObjectsReceived(10000ms);
    givenSignalsReset();

    whenPreviousMediaPlayerResourceChanged();

    thenProvidedPacketsAre({10000ms});
    thenMetadataProviderChangedTimes(0);
}

TEST_F(MediaPlayerAnalyticsProviderTest, resetMediaPlayerStopsProvidingMetadata)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenSignalsReset();

    whenMediaPlayerReset();

    thenMediaPlayerIs(nullptr);
    thenThereIsNoMetadataProvider();
    thenMetadataProviderChangedTimes(1);
    thenMediaPlayerChangedTimes(1);
}

TEST_F(MediaPlayerAnalyticsProviderTest, destroyedMediaPlayerStopsProvidingMetadata)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenSignalsReset();

    whenMediaPlayerDestroyed();

    thenMediaPlayerIs(nullptr);
    thenThereIsNoMetadataProvider();
    thenMetadataProviderChangedTimes(1);
    thenMediaPlayerChangedTimes(1);
}

// Lifetime of a released metadata provider.

TEST_F(MediaPlayerAnalyticsProviderTest, releasedMetadataProviderKeepsItsMetadata)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenMetadataProviderRemembered();

    whenDeactivated();

    thenRememberedMetadataProviderProvides({10000ms});
}

TEST_F(MediaPlayerAnalyticsProviderTest, metadataProviderOutlivesItsCreator)
{
    givenMediaPlayer();
    givenActiveProvider();
    givenObjectsReceived(10000ms);
    givenMetadataProviderRemembered();

    whenProviderDestroyed();

    thenRememberedMetadataProviderProvides({10000ms});
}

} // namespace nx::vms::client::core::test
