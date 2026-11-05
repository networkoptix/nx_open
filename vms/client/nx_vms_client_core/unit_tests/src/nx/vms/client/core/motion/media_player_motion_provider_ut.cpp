// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>

#include <gtest/gtest.h>

#include <QtTest/QSignalSpy>

#include <nx/analytics/abstract_metadata_consumer.h>
#include <nx/media/meta_data_packet.h>
#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/motion/helpers/media_player_motion_provider.h>
#include <nx/vms/client/core/motion/motion_grid.h>

namespace nx::vms::client::core::test {

namespace {

using namespace std::chrono;

constexpr int kMotionMaskSizeBytes = MotionGrid::kCellCount / 8;
constexpr microseconds kMotionDuration = 1s;

} // namespace

class TestMediaPlayerMotionProvider: public MediaPlayerMotionProvider
{
public:
    using MediaPlayerMotionProvider::metadataConsumer;
};

class MediaPlayerMotionProviderTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        provider = std::make_unique<TestMediaPlayerMotionProvider>();
        motionMaskChangedSpy = std::make_unique<QSignalSpy>(
            provider.get(), &MediaPlayerMotionProvider::motionMaskChanged);
        mediaPlayerChangedSpy = std::make_unique<QSignalSpy>(
            provider.get(), &MediaPlayerMotionProvider::mediaPlayerChanged);
    }

    virtual void TearDown() override
    {
        provider.reset();
        player.reset();
        otherPlayer.reset();
    }

    void givenMediaPlayer()
    {
        player = std::make_unique<MediaPlayer>();
        provider->setMediaPlayer(player.get());
    }

    void givenMotion(int channel = 0)
    {
        whenMotionReceived(channel);
        thenMotionMaskIsReceivedMotion(channel);
    }

    void givenSignalsReset()
    {
        motionMaskChangedSpy->clear();
        mediaPlayerChangedSpy->clear();
    }

    void whenMediaPlayerSet() { givenMediaPlayer(); }

    void whenSameMediaPlayerSet() { provider->setMediaPlayer(player.get()); }

    void givenMediaPlayerSwitched() { whenAnotherMediaPlayerSet(); }

    void whenAnotherMediaPlayerSet()
    {
        otherPlayer = std::make_unique<MediaPlayer>();
        provider->setMediaPlayer(otherPlayer.get());
    }

    void whenMediaPlayerReset() { provider->setMediaPlayer(nullptr); }

    void whenMediaPlayerDestroyed() { player.reset(); }

    void whenMotionReceived(int channel = 0)
    {
        const auto consumer = provider->metadataConsumer();
        ASSERT_TRUE(consumer);

        // Put the motion at the current player position, so the provider picks it.
        const auto timestamp = microseconds(milliseconds(provider->mediaPlayer()->position()));
        auto metadata = std::make_shared<QnMetaDataV1>(timestamp);
        metadata->m_duration = kMotionDuration.count();
        metadata->channelNumber = channel;
        metadata->setMotionAt(1, 2);
        metadata->setMotionAt(5, 7);

        receivedMotion = QByteArray(metadata->data(), int(metadata->dataSize()));
        receivedMotion.resize(kMotionMaskSizeBytes);

        consumer->processMetadata(metadata);
    }

    void givenResourceChanged() { whenResourceChanged(); }

    void whenResourceChanged() { emit player->resourceChanged(); }

    void whenPositionChanged() { emit player->positionChanged(); }

    void whenPreviousMediaPlayerResourceChanged()
    {
        emit player->resourceChanged();
        emit player->positionChanged();
    }

    void thenMotionMaskIsEmpty(int channel = 0)
    {
        const auto mask = provider->motionMask(channel);
        ASSERT_EQ(mask.size(), kMotionMaskSizeBytes);
        ASSERT_EQ(mask, QByteArray(kMotionMaskSizeBytes, 0));
    }

    void thenMotionMaskIsReceivedMotion(int channel = 0)
    {
        ASSERT_NE(receivedMotion, QByteArray(kMotionMaskSizeBytes, 0));
        ASSERT_EQ(provider->motionMask(channel), receivedMotion);
    }

    void thenMotionMaskChangeNotified() { ASSERT_EQ(motionMaskChangedSpy->count(), 1); }

    void thenMotionMaskChangeNotNotified() { ASSERT_EQ(motionMaskChangedSpy->count(), 0); }

    void thenMediaPlayerChangeNotified() { ASSERT_EQ(mediaPlayerChangedSpy->count(), 1); }

    void thenMediaPlayerChangeNotNotified() { ASSERT_EQ(mediaPlayerChangedSpy->count(), 0); }

    void thenMediaPlayerIs(MediaPlayer* expected) { ASSERT_EQ(provider->mediaPlayer(), expected); }

protected:
    std::unique_ptr<MediaPlayer> player;
    std::unique_ptr<MediaPlayer> otherPlayer;
    std::unique_ptr<TestMediaPlayerMotionProvider> provider;
    std::unique_ptr<QSignalSpy> motionMaskChangedSpy;
    std::unique_ptr<QSignalSpy> mediaPlayerChangedSpy;
    QByteArray receivedMotion;
};

TEST_F(MediaPlayerMotionProviderTest, hasNoMediaPlayerInitially)
{
    thenMediaPlayerIs(nullptr);
}

TEST_F(MediaPlayerMotionProviderTest, motionMaskIsEmptyWithoutMediaPlayer)
{
    thenMotionMaskIsEmpty();
}

TEST_F(MediaPlayerMotionProviderTest, motionMaskIsEmptyWithoutMotion)
{
    givenMediaPlayer();
    thenMotionMaskIsEmpty(/*channel*/ 0);
    thenMotionMaskIsEmpty(/*channel*/ 1);
}

TEST_F(MediaPlayerMotionProviderTest, settingMediaPlayerNotifiesChanges)
{
    whenMediaPlayerSet();
    thenMediaPlayerIs(player.get());
    thenMediaPlayerChangeNotified();
    thenMotionMaskChangeNotified();
}

TEST_F(MediaPlayerMotionProviderTest, settingSameMediaPlayerDoesNotNotifyChanges)
{
    givenMediaPlayer();
    givenSignalsReset();
    whenSameMediaPlayerSet();
    thenMediaPlayerChangeNotNotified();
    thenMotionMaskChangeNotNotified();
}

TEST_F(MediaPlayerMotionProviderTest, positionChangeNotifiesMotionMaskChange)
{
    givenMediaPlayer();
    givenSignalsReset();
    whenPositionChanged();
    thenMotionMaskChangeNotified();
}

TEST_F(MediaPlayerMotionProviderTest, resourceChangeNotifiesMotionMaskChange)
{
    givenMediaPlayer();
    givenSignalsReset();
    whenResourceChanged();
    thenMotionMaskChangeNotified();
    thenMotionMaskIsEmpty();
}

TEST_F(MediaPlayerMotionProviderTest, resettingMediaPlayerDropsMotion)
{
    givenMediaPlayer();
    givenMotion();
    givenSignalsReset();
    whenMediaPlayerReset();
    thenMediaPlayerIs(nullptr);
    thenMediaPlayerChangeNotified();
    thenMotionMaskChangeNotified();
    thenMotionMaskIsEmpty();
}

TEST_F(MediaPlayerMotionProviderTest, switchingMediaPlayerDropsMotion)
{
    givenMediaPlayer();
    givenMotion();
    givenSignalsReset();
    whenAnotherMediaPlayerSet();
    thenMediaPlayerIs(otherPlayer.get());
    thenMediaPlayerChangeNotified();
    thenMotionMaskChangeNotified();
    thenMotionMaskIsEmpty();
}

TEST_F(MediaPlayerMotionProviderTest, previousMediaPlayerIsNotTrackedAfterSwitching)
{
    givenMediaPlayer();
    givenMediaPlayerSwitched();
    givenSignalsReset();
    whenPreviousMediaPlayerResourceChanged();
    thenMotionMaskChangeNotNotified();
}

TEST_F(MediaPlayerMotionProviderTest, mediaPlayerDestructionResetsMediaPlayer)
{
    givenMediaPlayer();
    givenMotion();
    givenSignalsReset();
    whenMediaPlayerDestroyed();
    thenMediaPlayerIs(nullptr);
    thenMediaPlayerChangeNotified();
    thenMotionMaskChangeNotified();
    thenMotionMaskIsEmpty();
}

TEST_F(MediaPlayerMotionProviderTest, motionMaskReflectsReceivedMotion)
{
    givenMediaPlayer();
    whenMotionReceived();
    thenMotionMaskIsReceivedMotion();
}

TEST_F(MediaPlayerMotionProviderTest, motionMaskIsPerChannel)
{
    givenMediaPlayer();
    whenMotionReceived(/*channel*/ 1);
    thenMotionMaskIsReceivedMotion(/*channel*/ 1);
    thenMotionMaskIsEmpty(/*channel*/ 0);
}

TEST_F(MediaPlayerMotionProviderTest, positionChangeKeepsMotion)
{
    givenMediaPlayer();
    givenMotion();
    whenPositionChanged();
    thenMotionMaskIsReceivedMotion();
}

// MOBILE-3579: a paused player receives no new metadata after switching to another camera, so
// the motion cached for the previous camera must not be shown.
TEST_F(MediaPlayerMotionProviderTest, resourceChangeDropsMotionOfPreviousResource)
{
    givenMediaPlayer();
    givenMotion();
    whenResourceChanged();
    thenMotionMaskIsEmpty();
}

TEST_F(MediaPlayerMotionProviderTest, motionIsReceivedAfterResourceChange)
{
    givenMediaPlayer();
    givenMotion();
    givenResourceChanged();
    whenMotionReceived();
    thenMotionMaskIsReceivedMotion();
}

} // namespace nx::vms::client::core::test
