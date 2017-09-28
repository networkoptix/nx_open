#include "media_player.h"

#include <common/common_module.h>

#include <client_core/client_core_module.h>

namespace nx {
namespace client {
namespace mobile {

MediaPlayer::MediaPlayer(QObject* parent):
    base_type(parent)
{
    auto updateState = [this]
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

QString MediaPlayer::resourceId() const
{
    return m_resourceId;
}

void MediaPlayer::setResourceId(const QString& resourceId)
{
    if (m_resourceId == resourceId)
        return;

    m_resourceId = resourceId;
    emit resourceIdChanged();

    setSource(!resourceId.isEmpty()
        ? QUrl(lit("camera://media/") + resourceId)
        : QString());
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

QnCommonModule* MediaPlayer::commonModule() const
{
    return qnClientCoreModule->commonModule();
}

} // namespace mobile
} // namespace client
} // namespace nx
