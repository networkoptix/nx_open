#pragma once

#include <QtCore/QObject>

class QnCameraChunkProvider;

namespace nx::client::core { class MediaPlayer; }

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

    Q_PROPERTY(nx::client::core::MediaPlayer* mediaPlayer
        READ mediaPlayer
        WRITE setMediaPlayer
        NOTIFY mediaPlayerChanged)

    Q_PROPERTY(QnCameraChunkProvider* chunkProvider
        READ chunkProvider
        WRITE setChunkProvider
        NOTIFY chunkProviderChanged)

public:
    static void registerQmlType();

    MotionPlaybackMaskWatcher(QObject* parent = nullptr);
    virtual ~MotionPlaybackMaskWatcher();

    bool active() const;
    void setActive(bool value);

    core::MediaPlayer* mediaPlayer() const;
    void setMediaPlayer(core::MediaPlayer* player);

    QnCameraChunkProvider* chunkProvider() const;
    void setChunkProvider(QnCameraChunkProvider* provider);

signals:
    void mediaPlayerChanged();
    void chunkProviderChanged();
    void activeChanged();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::mobile
