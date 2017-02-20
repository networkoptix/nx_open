#include "media_player.h"

namespace nx {
namespace client {
namespace mobile {

MediaPlyer::MediaPlyer(QObject* parent):
    base_type(parent)
{
    auto updateState = [this]
        {
            const auto state = playbackState();
            const auto status = mediaStatus();

            const bool failed = ((m_loading || m_playing) && status == MediaStatus::NoMedia);
            if (m_failed != failed)
            {
                m_failed = failed;
                emit failedChanged();
            }

            const bool loading = (state == State::Playing && status == MediaStatus::Loading);
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

    connect(this, &MediaPlyer::playbackStateChanged, this, updateState);
    connect(this, &MediaPlyer::mediaStatusChanged, this, updateState);
}

MediaPlyer::~MediaPlyer()
{
}

QString MediaPlyer::resourceId() const
{
    return m_resourceId;
}

void MediaPlyer::setResourceId(const QString& resourceId)
{
    if (m_resourceId == resourceId)
        return;

    m_resourceId = resourceId;
    emit resourceIdChanged();

    setSource(!resourceId.isEmpty()
        ? QUrl(lit("camera://media/") + resourceId)
        : QString());
}

void MediaPlyer::playLive()
{
    setPosition(-1);
    play();
}

bool MediaPlyer::loading() const
{
    return m_loading;
}

bool MediaPlyer::playing() const
{
    return m_playing;
}

bool MediaPlyer::failed() const
{
    return m_failed;
}

} // namespace mobile
} // namespace client
} // namespace nx
