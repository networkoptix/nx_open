// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/media_player.h>
#include <nx/utils/uuid.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API MediaPlayer: public nx::media::Player
{
    Q_OBJECT

    /**
     * Resource to open.
     */
    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
    Q_PROPERTY(bool noVideoStreams READ noVideoStreams NOTIFY noVideoStreamsChanged)

    using base_type = nx::media::Player;

public:
    MediaPlayer(QObject* parent = nullptr);
    ~MediaPlayer();

    Q_INVOKABLE void playLive();

    bool loading() const;
    bool playing() const;
    bool failed() const;
    bool noVideoStreams() const;

    static void registerQmlTypes();

signals:
    void resourceIdChanged();
    void loadingChanged();
    void playingChanged();
    void failedChanged();
    void noVideoStreamsChanged();

protected:
    virtual void setResourceInternal(const QnResourcePtr& value) override;

private:
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

private:
    bool m_loading = false;
    bool m_playing = false;
    bool m_failed = false;
    bool m_noVideoStreams = false;
};

} // namespace nx::vms::client::core
