#pragma once

#include <QtCore/QObject>

class ChunkProvider;

namespace nx::vms::client::core { class MediaPlayer; }
namespace nx::client::core { class ChunkProvider; }

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

    Q_PROPERTY(nx::client::core::ChunkProvider* chunkProvider
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

    nx::client::core::ChunkProvider* chunkProvider() const;
    void setChunkProvider(nx::client::core::ChunkProvider* provider);

signals:
    void mediaPlayerChanged();
    void chunkProviderChanged();
    void activeChanged();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::mobile
