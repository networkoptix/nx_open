// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player_channel_adapter.h"

#include <QtCore/QSignalBlocker>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/QVideoSink>

#include "media_player.h"

namespace nx::vms::client::core {

MediaPlayerChannelAdapter::MediaPlayerChannelAdapter(QObject* parent):
    QObject(parent)
{
}

MediaPlayerChannelAdapter::~MediaPlayerChannelAdapter()
{
    setVideoSurface(nullptr);
}

QVideoSink* MediaPlayerChannelAdapter::videoSurface() const
{
    return m_videoSurface;
}

void MediaPlayerChannelAdapter::setVideoSurface(QVideoSink* videoSurface)
{
    if (m_videoSurface == videoSurface)
        return;

    unsetVideoSurface();
    m_videoSurface = videoSurface;
    applyVideoSurface();

    emit videoSurfaceChanged(m_videoSurface);
}

int MediaPlayerChannelAdapter::channel() const
{
    return m_channel;
}

void MediaPlayerChannelAdapter::setChannel(int channel)
{
    if (m_channel == channel)
        return;

    unsetVideoSurface();
    m_channel = channel;
    applyVideoSurface();

    emit channelChanged(m_channel);
}

MediaPlayer* MediaPlayerChannelAdapter::mediaPlayer() const
{
    return m_mediaPlayer;
}

void MediaPlayerChannelAdapter::setMediaPlayer(MediaPlayer* mediaPlayer)
{
    if (m_mediaPlayer == mediaPlayer)
        return;

    unsetVideoSurface();
    m_mediaPlayer = mediaPlayer;
    applyVideoSurface();

    emit mediaPlayerChanged(m_mediaPlayer);
}

void MediaPlayerChannelAdapter::redisplayCurrentFrame()
{
    if (!m_videoSurface)
        return;

    const QVideoFrame frame = m_videoSurface->videoFrame();
    if (!frame.isValid())
        return;

    // The sink ignores a frame equal to its current one, so it has to be reset first. Its signals
    // are blocked meanwhile: a video output would clear itself on the empty frame and reset its
    // source rect, which in turn resets the zoom of the scalable video items.
    {
        const QSignalBlocker blocker(m_videoSurface);
        m_videoSurface->setVideoFrame({});
    }
    m_videoSurface->setVideoFrame(frame);
}

void MediaPlayerChannelAdapter::unsetVideoSurface()
{
    if (m_mediaPlayer && m_videoSurface && m_channel >= 0)
        m_mediaPlayer->unsetVideoSurface(m_videoSurface, m_channel);
}

void MediaPlayerChannelAdapter::applyVideoSurface()
{
    if (m_mediaPlayer && m_videoSurface && m_channel >= 0)
        m_mediaPlayer->setVideoSurface(m_videoSurface, m_channel);
}

} // namespace nx::vms::client::core
