// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/media_player.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API MediaPlayer: public nx::media::Player
{
    Q_OBJECT

    Q_PROPERTY(nx::Uuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
    Q_PROPERTY(bool noVideoStreams READ noVideoStreams NOTIFY noVideoStreamsChanged)

    using base_type = nx::media::Player;

public:
    MediaPlayer(QObject* parent = nullptr);
    ~MediaPlayer();

    nx::Uuid resourceId() const;
    void setResourceId(const nx::Uuid& resourceId);

    Q_INVOKABLE void playLive();

    bool loading() const;
    bool playing() const;
    bool failed() const;
    bool noVideoStreams() const;

    static void registerQmlTypes();

protected:
    virtual QnCommonModule* commonModule() const override;

signals:
    void resourceIdChanged();
    void loadingChanged();
    void playingChanged();
    void failedChanged();
    void noVideoStreamsChanged();

private:
    nx::Uuid m_resourceId;
    bool m_loading = false;
    bool m_playing = false;
    bool m_failed = false;
    bool m_noVideoStreams = false;
};

} // namespace nx::vms::client::core
