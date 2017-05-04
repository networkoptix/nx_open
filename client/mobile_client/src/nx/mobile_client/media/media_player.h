#pragma once

#include <nx/media/media_player.h>

namespace nx {
namespace client {
namespace mobile {

class MediaPlayer: public nx::media::Player
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)

    using base_type = nx::media::Player;

public:
    MediaPlayer(QObject* parent = nullptr);
    ~MediaPlayer();

    QString resourceId() const;
    void setResourceId(const QString& resourceId);

    Q_INVOKABLE void playLive();

    bool loading() const;
    bool playing() const;
    bool failed() const;

protected:
    virtual QnCommonModule* commonModule() const override;

signals:
    void resourceIdChanged();
    void loadingChanged();
    void playingChanged();
    void failedChanged();

private:
    QString m_resourceId;
    bool m_loading = false;
    bool m_playing = false;
    bool m_failed = false;
};

} // namespace mobile
} // namespace client
} // namespace nx
