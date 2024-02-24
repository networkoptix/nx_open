// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player.h"

#include <QtQml/QtQml>

#include <core/resource/resource.h>

#include "media_player_channel_adapter.h"

namespace nx::vms::client::core {

MediaPlayer::MediaPlayer(QObject* parent):
    base_type(parent)
{
    const auto updateState =
        [this]
        {
            const auto state = playbackState();
            const auto status = mediaStatus();

            const bool noVideoStreams = status == MediaStatus::NoVideoStreams;
            if (noVideoStreams != m_noVideoStreams)
            {
                m_noVideoStreams = noVideoStreams;
                emit noVideoStreamsChanged();
            }

            const bool failed = (m_loading || m_playing) &&
                (status == MediaStatus::NoMedia || noVideoStreams);
            if (m_failed != failed)
            {
                m_failed = failed;
                emit failedChanged();
            }

            const bool loading = status == MediaStatus::Loading && state != State::Stopped;
            if (m_loading != loading)
            {
                m_loading = loading;
                emit loadingChanged();
            }

            const bool playing = (state == State::Playing && status == MediaStatus::Loaded);
            if (m_playing != playing)
            {
                m_playing = playing;
                emit playingChanged();
            }
        };

    connect(this, &MediaPlayer::playbackStateChanged, this, updateState);
    connect(this, &MediaPlayer::mediaStatusChanged, this, updateState);
}

MediaPlayer::~MediaPlayer()
{
}

void MediaPlayer::setResourceInternal(const QnResourcePtr& value)
{
    if (value)
        QQmlEngine::setObjectOwnership(value.get(), QQmlEngine::CppOwnership);

    base_type::setResourceInternal(value);
}

QnResource* MediaPlayer::rawResource() const
{
    return resource().data();
}

void MediaPlayer::setRawResource(QnResource* value)
{
    setResource(value ? value->toSharedPointer() : QnResourcePtr());
}

void MediaPlayer::playLive()
{
    setPosition(-1);
    play();
}

bool MediaPlayer::loading() const
{
    return m_loading;
}

bool MediaPlayer::playing() const
{
    return m_playing;
}

bool MediaPlayer::failed() const
{
    return m_failed;
}

bool MediaPlayer::noVideoStreams() const
{
    return m_noVideoStreams;
}

void MediaPlayer::registerQmlTypes()
{
    qmlRegisterType<MediaPlayer>("nx.vms.client.core", 1, 0, "MediaPlayer");
    qmlRegisterType<MediaPlayerChannelAdapter>(
        "nx.vms.client.core", 1, 0, "MediaPlayerChannelAdapter");
}

} // namespace nx::vms::client::core
