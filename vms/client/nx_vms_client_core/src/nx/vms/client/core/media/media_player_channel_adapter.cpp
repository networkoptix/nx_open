// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player_channel_adapter.h"

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

void MediaPlayerChannelAdapter::unsetVideoSurface()
{
    if (m_mediaPlayer && m_videoSurface && m_channel >= 0)
    {
        m_mediaPlayer->unsetVideoSurface(m_videoSurface, m_channel);
        m_videoSurface = nullptr;
    }
}

void MediaPlayerChannelAdapter::applyVideoSurface()
{
    if (m_mediaPlayer && m_videoSurface && m_channel >= 0)
        m_mediaPlayer->setVideoSurface(m_videoSurface, m_channel);
}

} // namespace nx::vms::client::core
