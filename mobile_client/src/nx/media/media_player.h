#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtMultimedia/QMediaPlayer>

/*
class QAbstractVideoSurface;

namespace nx
{
	namespace media
	{

		class PlayerPrivate;
		class Player : public QObject
		{
			Q_OBJECT

		public:
			enum class State {
				Disconnected,
				Connecting,
				Connected,
				Suspended
			};

			enum class MediaStatus
			{
				Unknown,
				NoMedia,
				Loading,
				Loaded,
				Stalled,
				Buffering,
				Buffered,
				EndOfMedia,
				InvalidMedia
			};

			Q_ENUMS(State)
			Q_ENUMS(MediaStatus)

			Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
			
			Q_PROPERTY(QAbstractVideoSurface * videoSurface READ videoSurface WRITE setVideoSurface NOTIFY videoSurfaceChanged)

			Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)

			Q_PROPERTY(Player::State playbackState READ playbackState NOTIFY playbackStateChanged)
			Q_PROPERTY(QMediaPlayer::MediaStatus mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)

			Q_PROPERTY(bool reconnectOnPlay READ reconnectOnPlay WRITE setReconnectOnPlay NOTIFY reconnectOnPlayChanged)

		public:
			Player(QObject *parent = nullptr);
			~Player();

			QMediaPlayer::State playbackState() const;

			QMediaPlayer::MediaStatus mediaStatus() const;

			QUrl source() const;

			QAbstractVideoSurface *videoSurface() const;

			qint64 position() const;
			void setPosition(qint64 value) const;

			bool reconnectOnPlay() const;
			void setReconnectOnPlay(bool reconnectOnPlay);

		public slots:
			void play();
			void pause();
			void stop();

			void setSource(const QUrl &source);
			void setVideoSurface(QAbstractVideoSurface *videoSurface);

		signals:
			void playbackStateChanged();
			void sourceChanged();
			void videoSurfaceChanged();
			void positionChanged();
			void playbackFinished();
			void mediaStatusChanged();

		private:
			QScopedPointer<PlayerPrivate> d_ptr;
			Q_DECLARE_PRIVATE(PlayerPrivate)
		};

	}
	*/
}
