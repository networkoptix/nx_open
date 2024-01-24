// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player_motion_provider.h"

#include <chrono>

#include <QtQml/QtQml>

#include <nx/vms/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/motion/motion_grid.h>

namespace nx::vms::client::core {

//-------------------------------------------------------------------------------------------------
// MediaPlayerMotionProvider::Private

class MediaPlayerMotionProvider::Private
{
public:
    MediaPlayer* mediaPlayer = nullptr;
    ConsumingMotionMetadataProvider metadataProvider;
};

//-------------------------------------------------------------------------------------------------
// MediaPlayerMotionProvider

MediaPlayerMotionProvider::MediaPlayerMotionProvider(QObject* parent):
    QObject(parent),
    d(new Private())
{
}

MediaPlayerMotionProvider::~MediaPlayerMotionProvider()
{
    setMediaPlayer(nullptr);
}

MediaPlayer* MediaPlayerMotionProvider::mediaPlayer() const
{
    return d->mediaPlayer;
}

void MediaPlayerMotionProvider::setMediaPlayer(MediaPlayer* value)
{
    if (d->mediaPlayer == value)
        return;

    if (d->mediaPlayer)
    {
        d->mediaPlayer->removeMetadataConsumer(d->metadataProvider.metadataConsumer());
        d->mediaPlayer->disconnect(this);
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

        connect(d->mediaPlayer, &MediaPlayer::sourceChanged,
            this, &MediaPlayerMotionProvider::motionMaskChanged);
        connect(d->mediaPlayer, &MediaPlayer::positionChanged,
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
    qmlRegisterType<MediaPlayerMotionProvider>(
        "nx.vms.client.core", 1, 0, "MediaPlayerMotionProvider");
}

} // namespace nx::vms::client::core
