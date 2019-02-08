#include "media_player_motion_provider.h"

#include <chrono>

#include <QtQml/QtQml>

#include <nx/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/client/core/motion/motion_grid.h>
#include <nx/media/media_player.h>

namespace nx::vms::client::core {

//-------------------------------------------------------------------------------------------------
// MediaPlayerMotionProvider::Private

class MediaPlayerMotionProvider::Private: public QObject
{
    MediaPlayerMotionProvider* const q = nullptr;

public:
    Private(MediaPlayerMotionProvider* q): QObject(), q(q) {}

public:
    media::Player* mediaPlayer = nullptr;
    ConsumingMotionMetadataProvider metadataProvider;
};

//-------------------------------------------------------------------------------------------------
// MediaPlayerMotionProvider

MediaPlayerMotionProvider::MediaPlayerMotionProvider(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
}

MediaPlayerMotionProvider::~MediaPlayerMotionProvider()
{
    setMediaPlayer(nullptr);
}

media::Player* MediaPlayerMotionProvider::mediaPlayer() const
{
    return d->mediaPlayer;
}

void MediaPlayerMotionProvider::setMediaPlayer(media::Player* value)
{
    if (d->mediaPlayer == value)
        return;

    if (d->mediaPlayer)
    {
        d->mediaPlayer->removeMetadataConsumer(d->metadataProvider.metadataConsumer());
        d->mediaPlayer->disconnect(d.data());
    }

    d->mediaPlayer = value;

    if (d->mediaPlayer)
    {
        connect(d->mediaPlayer, &QObject::destroyed, this,
            [this]()
            {
                d->mediaPlayer = nullptr;
                emit mediaPlayerChanged();
            });

        connect(d->mediaPlayer, &media::Player::sourceChanged,
            this, &MediaPlayerMotionProvider::motionMaskChanged);
        connect(d->mediaPlayer, &media::Player::positionChanged,
            this, &MediaPlayerMotionProvider::motionMaskChanged);

        d->mediaPlayer->addMetadataConsumer(d->metadataProvider.metadataConsumer());
    }

    emit motionMaskChanged();
}

QByteArray MediaPlayerMotionProvider::motionMask(int channel) const
{
    static constexpr int kMotionMaskSizeBytes = MotionGrid::kCellCount / 8;
    static QByteArray kEmptyMotionMask(kMotionMaskSizeBytes, 0);

    if (!d->mediaPlayer)
        return kEmptyMotionMask;

    using namespace std::chrono;
    const auto timestamp = microseconds(milliseconds(d->mediaPlayer->position()));
    const auto metadata = d->metadataProvider.metadata(timestamp, channel);
    if (!metadata)
        return kEmptyMotionMask;

    QByteArray motionMask(metadata->data(), int(metadata->dataSize()));
    motionMask.resize(kMotionMaskSizeBytes);
    return motionMask;
}

void MediaPlayerMotionProvider::registerQmlType()
{
    qmlRegisterType<MediaPlayerMotionProvider>("nx.client.core", 1, 0, "MediaPlayerMotionProvider");
}

} // namespace nx::vms::client::core
