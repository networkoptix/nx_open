// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

Q_MOC_INCLUDE("nx/vms/client/core/media/media_player.h")
Q_MOC_INCLUDE("nx/vms/client/core/media/chunk_provider.h")

class ChunkProvider;

namespace nx::vms::client::core { class MediaPlayer; }
namespace nx::vms::client::core { class ChunkProvider; }

namespace nx::client::mobile
{

class MotionPlaybackMaskWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool active
        READ active
        WRITE setActive
        NOTIFY activeChanged)

    Q_PROPERTY(nx::vms::client::core::MediaPlayer* mediaPlayer
        READ mediaPlayer
        WRITE setMediaPlayer
        NOTIFY mediaPlayerChanged)

    Q_PROPERTY(nx::vms::client::core::ChunkProvider* chunkProvider
        READ chunkProvider
        WRITE setChunkProvider
        NOTIFY chunkProviderChanged)

public:
    static void registerQmlType();

    MotionPlaybackMaskWatcher(QObject* parent = nullptr);
    virtual ~MotionPlaybackMaskWatcher();

    bool active() const;
    void setActive(bool value);

    nx::vms::client::core::MediaPlayer* mediaPlayer() const;
    void setMediaPlayer(nx::vms::client::core::MediaPlayer* player);

    nx::vms::client::core::ChunkProvider* chunkProvider() const;
    void setChunkProvider(nx::vms::client::core::ChunkProvider* provider);

signals:
    void mediaPlayerChanged();
    void chunkProviderChanged();
    void activeChanged();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::mobile
