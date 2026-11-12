// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player_motion_provider.h"

#include <chrono>
#include <memory>

#include <QtQml/QtQml>

#include <nx/analytics/signaling_metadata_consumer.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/media/consuming_motion_metadata_provider.h>
#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/motion/motion_grid.h>

namespace nx::vms::client::core {

//-------------------------------------------------------------------------------------------------
// MediaPlayerMotionProvider::Private

class MediaPlayerMotionProvider::Private
{
public:
    MediaPlayerMotionProvider* const q;
    MediaPlayer* mediaPlayer = nullptr;

    /** Recreated for every resource, so the metadata cached for another resource is dropped. */
    std::unique_ptr<ConsumingMotionMetadataProvider> metadataProvider;

    /**
     * Added to the media player while there is one, and passes the received metadata to the
     * current metadata provider. It is the same for all resources, so a resource change replaces
     * only the metadata provider and does not change what the player requests.
     */
    const QSharedPointer<nx::analytics::SignalingMetadataConsumer> metadataConsumer{
        new nx::analytics::SignalingMetadataConsumer(MetadataType::Motion)};

    /** The motion mask may change with the received metadata even when the position doesn't. */
    nx::utils::PendingOperation motionMaskChangeNotification{
        [this]() { emit q->motionMaskChanged(); }, /*intervalMs*/ 0};

    explicit Private(MediaPlayerMotionProvider* q): q(q) {}

    void resetMetadataProvider()
    {
        metadataProvider.reset(mediaPlayer ? new ConsumingMotionMetadataProvider() : nullptr);
    }

    void handleMetadata(const QnConstAbstractCompressedMetadataPtr& metadata)
    {
        if (!metadataProvider)
            return;

        metadataProvider->metadataConsumer()->processMetadata(metadata);
        motionMaskChangeNotification.requestOperation();
    }
};

//-------------------------------------------------------------------------------------------------
// MediaPlayerMotionProvider

MediaPlayerMotionProvider::MediaPlayerMotionProvider(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    // Once per event loop iteration, however many requests were made in it.
    d->motionMaskChangeNotification.setFlags(nx::utils::PendingOperation::NoFlags);

    connect(d->metadataConsumer.get(),
        &nx::analytics::SignalingMetadataConsumer::metadataReceived,
        this,
        [this](const QnConstAbstractCompressedMetadataPtr& metadata)
        { d->handleMetadata(metadata); });
}

MediaPlayerMotionProvider::~MediaPlayerMotionProvider()
{
    if (d->mediaPlayer)
        d->mediaPlayer->removeMetadataConsumer(d->metadataConsumer);
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
        d->mediaPlayer->removeMetadataConsumer(d->metadataConsumer);
        d->mediaPlayer->disconnect(this);
    }

    d->mediaPlayer = value;

    if (d->mediaPlayer)
    {
        connect(d->mediaPlayer, &QObject::destroyed, this,
            [this]()
            {
                d->mediaPlayer = nullptr;
                d->metadataProvider.reset();
                emit mediaPlayerChanged();
                emit motionMaskChanged();
            });

        connect(d->mediaPlayer,
            &MediaPlayer::resourceChanged,
            this,
            [this]()
            {
                d->resetMetadataProvider();
                emit motionMaskChanged();
            });
        connect(d->mediaPlayer,
            &MediaPlayer::positionChanged,
            this,
            &MediaPlayerMotionProvider::motionMaskChanged);

        d->mediaPlayer->addMetadataConsumer(d->metadataConsumer);
    }

    d->resetMetadataProvider();
    emit mediaPlayerChanged();
    emit motionMaskChanged();
}

QByteArray MediaPlayerMotionProvider::motionMask(int channel) const
{
    static constexpr int kMotionMaskSizeBytes = MotionGrid::kCellCount / 8;
    static QByteArray kEmptyMotionMask(kMotionMaskSizeBytes, 0);

    if (!d->mediaPlayer || !d->metadataProvider)
        return kEmptyMotionMask;

    using namespace std::chrono;
    const auto timestamp = microseconds(milliseconds(d->mediaPlayer->position()));
    const auto metadata = d->metadataProvider->metadata(timestamp, channel);
    if (!metadata)
        return kEmptyMotionMask;

    QByteArray motionMask(metadata->data(), int(metadata->dataSize()));
    motionMask.resize(kMotionMaskSizeBytes);
    return motionMask;
}

QSharedPointer<nx::analytics::AbstractMetadataConsumer> MediaPlayerMotionProvider::
    metadataConsumer() const
{
    return d->mediaPlayer ? d->metadataConsumer : nullptr;
}

void MediaPlayerMotionProvider::registerQmlType()
{
    qmlRegisterType<MediaPlayerMotionProvider>(
        "nx.vms.client.core", 1, 0, "MediaPlayerMotionProvider");
}

} // namespace nx::vms::client::core
