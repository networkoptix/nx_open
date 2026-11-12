// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/media/analytics_fwd.h>

Q_MOC_INCLUDE("nx/vms/client/core/media/media_player.h")

namespace nx::analytics { class AbstractMetadataConsumer; }

namespace nx::vms::client::core {

class MediaPlayer;

/**
 * Provides analytics object metadata received by the media player in its stream. The metadata is
 * requested from the server only while the provider is active.
 */
class NX_VMS_CLIENT_CORE_API MediaPlayerAnalyticsProvider: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::core::MediaPlayer* mediaPlayer READ mediaPlayer WRITE
            setMediaPlayer NOTIFY mediaPlayerChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

    /**
     * Null while inactive. Recreated when the resource changes, so it never provides metadata of
     * another resource.
     */
    Q_PROPERTY(nx::vms::client::core::AbstractAnalyticsMetadataProviderPtr metadataProvider READ
            metadataProvider NOTIFY metadataProviderChanged)

public:
    explicit MediaPlayerAnalyticsProvider(QObject* parent = nullptr);
    virtual ~MediaPlayerAnalyticsProvider() override;

    MediaPlayer* mediaPlayer() const;
    void setMediaPlayer(MediaPlayer* value);

    bool active() const;
    void setActive(bool value);

    AbstractAnalyticsMetadataProviderPtr metadataProvider() const;

    static void registerQmlType();

signals:
    void mediaPlayerChanged();
    void activeChanged();
    void metadataProviderChanged();

    /**
     * New metadata has been received into the metadata provider. Emitted once per event loop
     * iteration, however many packets were received in it.
     */
    void metadataReceived();

protected:
    /** Consumer added to the media player, null while no metadata is provided. For tests. */
    QSharedPointer<nx::analytics::AbstractMetadataConsumer> metadataConsumer() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
