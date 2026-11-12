// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QRectF>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/QVideoFrameFormat>
#include <QtMultimedia/QVideoSink>
#include <QtQml/QJSValue>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QSignalSpy>

#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/media/media_player_channel_adapter.h>

namespace nx::vms::client::core::test {

namespace {

// A video output which records its source rect on every change notification.
constexpr char kVideoOutputQml[] = R"(
    import QtMultimedia

    VideoOutput
    {
        property var notifiedSourceRects: []

        // A copy is stored: sourceRect read in JS is a reference to the current value.
        onSourceRectChanged: notifiedSourceRects.push(Qt.rect(
            sourceRect.x, sourceRect.y, sourceRect.width, sourceRect.height))
    }
)";

} // namespace

class MediaPlayerChannelAdapterTest: public ::testing::Test
{
protected:
    void givenAdapterConnecting(MediaPlayer* player, int channel, QVideoSink* surface)
    {
        adapter->setMediaPlayer(player);
        adapter->setChannel(channel);
        adapter->setVideoSurface(surface);
    }

    void givenAdapterWithoutSurface()
    {
        adapter->setMediaPlayer(&player1);
        adapter->setChannel(0);
    }

    void givenDestructiblePlayerDestroyed() { destructiblePlayer.reset(); }

    void givenDestructibleSurfaceDestroyed() { destructibleSurface.reset(); }

    void whenMediaPlayerChanged(MediaPlayer* player) { adapter->setMediaPlayer(player); }

    void whenChannelChanged(int channel) { adapter->setChannel(channel); }

    void whenVideoSurfaceChanged(QVideoSink* surface) { adapter->setVideoSurface(surface); }

    void whenDestructibleSurfaceDestroyed() { destructibleSurface.reset(); }

    void whenAdapterDestroyed() { adapter.reset(); }

    void whenCurrentFrameRedisplayed() { adapter->redisplayCurrentFrame(); }

    void thenPlayerRendersTo(MediaPlayer* player, int channel, QVideoSink* surface)
    {
        ASSERT_EQ(player->videoSurface(channel), surface);
    }

    void thenAdapterReportsVideoSurface(QVideoSink* surface)
    {
        ASSERT_EQ(adapter->videoSurface(), surface);
    }

protected:
    // Declared before the players and the adapter, so they are destroyed after them.
    QVideoSink surface1;
    QVideoSink surface2;
    std::unique_ptr<QVideoSink> destructibleSurface = std::make_unique<QVideoSink>();

    // Declared before the adapter, so they are destroyed after it.
    MediaPlayer player1;
    MediaPlayer player2;
    std::unique_ptr<MediaPlayer> destructiblePlayer = std::make_unique<MediaPlayer>();

    std::unique_ptr<MediaPlayerChannelAdapter> adapter =
        std::make_unique<MediaPlayerChannelAdapter>();
};

/** Tests of the frame redisplaying, which needs video sinks able to hold a frame. */
class MediaPlayerChannelAdapterFrameTest: public MediaPlayerChannelAdapterTest
{
protected:
    virtual void SetUp() override
    {
        // Without a Qt Multimedia backend a video sink has no implementation and ignores frames.
        // That is a failure rather than a skip, so the tests can't lose their coverage unnoticed.
        QVideoSink probe;
        probe.setVideoFrame(frame);
        ASSERT_EQ(probe.videoFrame(), frame)
            << "Video sinks can't hold frames: no Qt Multimedia backend available";
    }

    void givenSurfaceHoldingFrame(QVideoSink* surface) { surface->setVideoFrame(frame); }

    void givenDeliveredFramesWatched(QVideoSink* surface)
    {
        deliveredFrames = std::make_unique<QSignalSpy>(surface, &QVideoSink::videoFrameChanged);
    }

    void thenFrameDeliveredOnce()
    {
        ASSERT_EQ(deliveredFrames->count(), 1);

        const auto delivered = deliveredFrames->first().first().value<QVideoFrame>();
        ASSERT_TRUE(delivered.isValid());
        ASSERT_EQ(delivered, frame);
    }

    void thenNoFrameDelivered() { ASSERT_EQ(deliveredFrames->count(), 0); }

    void thenSurfaceHoldsFrame(QVideoSink* surface) { ASSERT_EQ(surface->videoFrame(), frame); }

protected:
    const QVideoFrame frame{QVideoFrameFormat(QSize(4, 4), QVideoFrameFormat::Format_RGBA8888)};
    std::unique_ptr<QSignalSpy> deliveredFrames;
};

/** Tests of the frame redisplaying with a QML video output connected to the adapter. */
class MediaPlayerChannelAdapterVideoOutputTest: public MediaPlayerChannelAdapterFrameTest
{
protected:
    virtual void SetUp() override
    {
        MediaPlayerChannelAdapterFrameTest::SetUp();
        if (HasFatalFailure())
            return;

        QQmlComponent component(&qmlEngine);
        component.setData(kVideoOutputQml, QUrl());

        videoOutput.reset(component.create());
        ASSERT_TRUE(videoOutput) << component.errorString().toStdString();
    }

    QVideoSink* videoOutputSink() const
    {
        return videoOutput->property("videoSink").value<QVideoSink*>();
    }

    QJSValue notifiedSourceRects() const
    {
        return videoOutput->property("notifiedSourceRects").value<QJSValue>();
    }

    int notifiedSourceRectCount() const
    {
        return notifiedSourceRects().property("length").toInt();
    }

    void givenVideoOutputShowingFrame()
    {
        givenSurfaceHoldingFrame(videoOutputSink());
        ASSERT_EQ(videoOutput->property("sourceRect").toRectF(), frameRect());
        sourceRectCountBeforeAction = notifiedSourceRectCount();
    }

    void thenVideoOutputSourceRectNeverChanged()
    {
        const auto sourceRects = notifiedSourceRects();
        for (int i = sourceRectCountBeforeAction; i < notifiedSourceRectCount(); ++i)
            ASSERT_EQ(sourceRects.property(i).toVariant().toRectF(), frameRect()) << "at " << i;

        ASSERT_EQ(videoOutput->property("sourceRect").toRectF(), frameRect());
    }

    QRectF frameRect() const { return QRectF(QPointF(0, 0), frame.size()); }

protected:
    // Declared before the video output, so it is destroyed after it.
    QQmlEngine qmlEngine;
    std::unique_ptr<QObject> videoOutput;
    int sourceRectCountBeforeAction = 0;
};

TEST_F(MediaPlayerChannelAdapterTest, connectsSurfaceToPlayerChannel)
{
    givenAdapterConnecting(&player1, 1, &surface1);

    thenPlayerRendersTo(&player1, 1, &surface1);
}

TEST_F(MediaPlayerChannelAdapterTest, movesSurfaceToNewMediaPlayer)
{
    givenAdapterConnecting(&player1, 0, &surface1);

    whenMediaPlayerChanged(&player2);

    thenPlayerRendersTo(&player1, 0, nullptr);
    thenPlayerRendersTo(&player2, 0, &surface1);
    thenAdapterReportsVideoSurface(&surface1);
}

TEST_F(MediaPlayerChannelAdapterTest, connectsSurfaceToNewPlayerAfterPlayerDestroyed)
{
    givenAdapterConnecting(destructiblePlayer.get(), 0, &surface1);
    givenDestructiblePlayerDestroyed();

    whenMediaPlayerChanged(&player2);

    thenPlayerRendersTo(&player2, 0, &surface1);
    thenAdapterReportsVideoSurface(&surface1);
}

TEST_F(MediaPlayerChannelAdapterTest, movesSurfaceToNewChannel)
{
    givenAdapterConnecting(&player1, 0, &surface1);

    whenChannelChanged(1);

    thenPlayerRendersTo(&player1, 0, nullptr);
    thenPlayerRendersTo(&player1, 1, &surface1);
    thenAdapterReportsVideoSurface(&surface1);
}

TEST_F(MediaPlayerChannelAdapterTest, reconnectsSurfaceWhenChannelBecomesValidAgain)
{
    givenAdapterConnecting(&player1, 0, &surface1);

    whenChannelChanged(-1);
    thenPlayerRendersTo(&player1, 0, nullptr);

    whenChannelChanged(0);
    thenPlayerRendersTo(&player1, 0, &surface1);
}

TEST_F(MediaPlayerChannelAdapterTest, replacesSurface)
{
    givenAdapterConnecting(&player1, 0, &surface1);

    whenVideoSurfaceChanged(&surface2);

    thenPlayerRendersTo(&player1, 0, &surface2);
    thenAdapterReportsVideoSurface(&surface2);
}

TEST_F(MediaPlayerChannelAdapterTest, disconnectsSurfaceWhenUnset)
{
    givenAdapterConnecting(&player1, 0, &surface1);

    whenVideoSurfaceChanged(nullptr);

    thenPlayerRendersTo(&player1, 0, nullptr);
    thenAdapterReportsVideoSurface(nullptr);
}

TEST_F(MediaPlayerChannelAdapterTest, disconnectsSurfaceWhenDestroyed)
{
    givenAdapterConnecting(&player1, 0, &surface1);

    whenAdapterDestroyed();

    thenPlayerRendersTo(&player1, 0, nullptr);
}

TEST_F(MediaPlayerChannelAdapterTest, forgetsDestroyedSurface)
{
    givenAdapterConnecting(&player1, 0, destructibleSurface.get());

    whenDestructibleSurfaceDestroyed();

    thenAdapterReportsVideoSurface(nullptr);
}

TEST_F(MediaPlayerChannelAdapterTest, ignoresRedisplayWithoutSurface)
{
    givenAdapterWithoutSurface();

    whenCurrentFrameRedisplayed();

    thenAdapterReportsVideoSurface(nullptr);
}

TEST_F(MediaPlayerChannelAdapterTest, ignoresRedisplayAfterSurfaceDestroyed)
{
    givenAdapterConnecting(&player1, 0, destructibleSurface.get());
    givenDestructibleSurfaceDestroyed();

    whenCurrentFrameRedisplayed();

    thenAdapterReportsVideoSurface(nullptr);
}

TEST_F(MediaPlayerChannelAdapterFrameTest, redisplaysCurrentFrameOnce)
{
    givenAdapterConnecting(&player1, 0, &surface1);
    givenSurfaceHoldingFrame(&surface1);
    givenDeliveredFramesWatched(&surface1);

    whenCurrentFrameRedisplayed();

    thenFrameDeliveredOnce();
    thenSurfaceHoldsFrame(&surface1);
}

TEST_F(MediaPlayerChannelAdapterVideoOutputTest, keepsSourceRectOnRedisplay)
{
    givenAdapterConnecting(&player1, 0, videoOutputSink());
    givenVideoOutputShowingFrame();

    whenCurrentFrameRedisplayed();

    thenVideoOutputSourceRectNeverChanged();
}

TEST_F(MediaPlayerChannelAdapterFrameTest, doesNotRedisplayWithoutFrame)
{
    givenAdapterConnecting(&player1, 0, &surface1);
    givenDeliveredFramesWatched(&surface1);

    whenCurrentFrameRedisplayed();

    thenNoFrameDelivered();
}

} // namespace nx::vms::client::core::test
