// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

class QAbstractVideoSurface;

namespace nx::vms::client::core {

class MediaPlayer;

class MediaPlayerChannelAdapter: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractVideoSurface* videoSurface READ videoSurface WRITE setVideoSurface
        NOTIFY videoSurfaceChanged)
    Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY channelChanged)
    Q_PROPERTY(nx::vms::client::core::MediaPlayer* mediaPlayer READ mediaPlayer
        WRITE setMediaPlayer NOTIFY mediaPlayerChanged)

public:
    MediaPlayerChannelAdapter(QObject* parent = nullptr);

    QAbstractVideoSurface* videoSurface() const;
    void setVideoSurface(QAbstractVideoSurface* videoSurface);

    int channel() const;
    void setChannel(int channel);

    MediaPlayer* mediaPlayer() const;
    void setMediaPlayer(MediaPlayer* mediaPlayer);

signals:
    void videoSurfaceChanged(QAbstractVideoSurface* videoSurface);
    void channelChanged(int channel);
    void mediaPlayerChanged(nx::vms::client::core::MediaPlayer* mediaPlayer);

private:
    void unsetVideoSurface();
    void applyVideoSurface();

private:
    QAbstractVideoSurface* m_videoSurface = nullptr;
    int m_channel = -1;
    QPointer<MediaPlayer> m_mediaPlayer;
};

} // namespace nx::vms::client::core
