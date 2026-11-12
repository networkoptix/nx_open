// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player_analytics_provider.h"

#include <QtCore/QPointer>
#include <QtQml/QtQml>

#include <nx/analytics/signaling_metadata_consumer.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/core/media/consuming_analytics_metadata_provider.h>
#include <nx/vms/client/core/media/media_player.h>

namespace nx::vms::client::core {

struct MediaPlayerAnalyticsProvider::Private
{
    MediaPlayerAnalyticsProvider* const q;
    QPointer<MediaPlayer> mediaPlayer;
    bool active = false;
    QSharedPointer<ConsumingAnalyticsMetadataProvider> metadataProvider;

    /**
     * Added to the media player while metadata is provided, and passes the received metadata to
     * the current metadata provider. It is the same for all resources, so a resource change
     * replaces only the metadata provider and does not change what the player requests.
     */
    const QSharedPointer<nx::analytics::SignalingMetadataConsumer> metadataConsumer{
        new nx::analytics::SignalingMetadataConsumer(MetadataType::ObjectDetection)};

    nx::utils::PendingOperation metadataReceivedNotification{
        [this]() { emit q->metadataReceived(); }, /*intervalMs*/ 0};

    bool effectivelyActive() const { return mediaPlayer && active; }

    void updateMetadataConsumer()
    {
        if (!mediaPlayer)
            return;

        if (active)
            mediaPlayer->addMetadataConsumer(metadataConsumer);
        else
            mediaPlayer->removeMetadataConsumer(metadataConsumer);
    }

    /** Replaces the metadata provider, so it never has the metadata of another resource. */
    void updateMetadataProvider()
    {
        const bool hadMetadataProvider = !metadataProvider.isNull();

        metadataProvider.reset(
            effectivelyActive() ? new ConsumingAnalyticsMetadataProvider() : nullptr);

        if (hadMetadataProvider || metadataProvider)
            emit q->metadataProviderChanged();
    }

    void handleMetadata(const QnConstAbstractCompressedMetadataPtr& metadata)
    {
        if (!metadataProvider)
            return;

        metadataProvider->metadataConsumer()->processMetadata(metadata);
        metadataReceivedNotification.requestOperation();
    }
};

MediaPlayerAnalyticsProvider::MediaPlayerAnalyticsProvider(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
    // Once per event loop iteration, however many requests were made in it.
    d->metadataReceivedNotification.setFlags(nx::utils::PendingOperation::NoFlags);

    connect(d->metadataConsumer.get(),
        &nx::analytics::SignalingMetadataConsumer::metadataReceived,
        this,
        [this](const QnConstAbstractCompressedMetadataPtr& metadata)
        { d->handleMetadata(metadata); });
}

MediaPlayerAnalyticsProvider::~MediaPlayerAnalyticsProvider()
{
    if (d->mediaPlayer)
        d->mediaPlayer->removeMetadataConsumer(d->metadataConsumer);
}

MediaPlayer* MediaPlayerAnalyticsProvider::mediaPlayer() const
{
    return d->mediaPlayer;
}

void MediaPlayerAnalyticsProvider::setMediaPlayer(MediaPlayer* value)
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
        connect(d->mediaPlayer,
            &QObject::destroyed,
            this,
            [this]()
            {
                d->metadataProvider.reset();
                emit metadataProviderChanged();
                emit mediaPlayerChanged();
            });

        connect(d->mediaPlayer,
            &MediaPlayer::resourceChanged,
            this,
            [this]() { d->updateMetadataProvider(); });
    }

    d->updateMetadataConsumer();
    d->updateMetadataProvider();
    emit mediaPlayerChanged();
}

bool MediaPlayerAnalyticsProvider::active() const
{
    return d->active;
}

void MediaPlayerAnalyticsProvider::setActive(bool value)
{
    if (d->active == value)
        return;

    d->active = value;
    d->updateMetadataConsumer();
    d->updateMetadataProvider();
    emit activeChanged();
}

AbstractAnalyticsMetadataProviderPtr MediaPlayerAnalyticsProvider::metadataProvider() const
{
    return d->metadataProvider;
}

QSharedPointer<nx::analytics::AbstractMetadataConsumer> MediaPlayerAnalyticsProvider::
    metadataConsumer() const
{
    return d->effectivelyActive() ? d->metadataConsumer : nullptr;
}

void MediaPlayerAnalyticsProvider::registerQmlType()
{
    qmlRegisterType<MediaPlayerAnalyticsProvider>(
        "nx.vms.client.core", 1, 0, "MediaPlayerAnalyticsProvider");
}

} // namespace nx::vms::client::core
