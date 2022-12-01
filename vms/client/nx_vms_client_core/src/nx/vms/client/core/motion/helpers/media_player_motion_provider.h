// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

Q_MOC_INCLUDE("nx/vms/client/core/media/media_player.h")

namespace nx::vms::client::core {

class MediaPlayer;

/**
 * Provides access to metadata acquired by specified media player from rtsp streams.
 */
class MediaPlayerMotionProvider: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::core::MediaPlayer* mediaPlayer
        READ mediaPlayer WRITE setMediaPlayer NOTIFY mediaPlayerChanged)

public:
    MediaPlayerMotionProvider(QObject* parent = nullptr);
    virtual ~MediaPlayerMotionProvider() override;

    MediaPlayer* mediaPlayer() const;
    void setMediaPlayer(MediaPlayer* value);

    // Unaligned binary motion mask, MotionGrid::kGridSize bits, each byte MSB first.
    Q_INVOKABLE QByteArray motionMask(int channel) const;

    static void registerQmlType();

signals:
    void mediaPlayerChanged();
    void motionMaskChanged();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::core
