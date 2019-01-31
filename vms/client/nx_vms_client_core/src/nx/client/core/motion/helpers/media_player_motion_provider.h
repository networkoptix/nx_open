#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

namespace nx::media { class Player; }

namespace nx::vms::client::core {

/**
 * Provides access to metadata acquired by specified media player from rtsp streams.
 */
class MediaPlayerMotionProvider: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::media::Player* mediaPlayer
        READ mediaPlayer WRITE setMediaPlayer NOTIFY mediaPlayerChanged)

public:
    MediaPlayerMotionProvider(QObject* parent = nullptr);
    virtual ~MediaPlayerMotionProvider() override;

    media::Player* mediaPlayer() const;
    void setMediaPlayer(media::Player* value);

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
