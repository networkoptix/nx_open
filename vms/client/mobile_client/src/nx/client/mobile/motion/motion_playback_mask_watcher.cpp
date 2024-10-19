// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_playback_mask_watcher.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/media/media_player.h>
#include <nx/vms/client/core/media/chunk_provider.h>

namespace nx::client::mobile
{

class MotionPlaybackMaskWatcher::Private: public QObject
{
public:
    Private(MotionPlaybackMaskWatcher* q);

    bool active() const;
    void setActive(bool value);

    nx::vms::client::core::MediaPlayer* mediaPlayer();
    void setMediaPlayer(nx::vms::client::core::MediaPlayer* value);

    nx::vms::client::core::ChunkProvider* chunkProvider() const;
    void setChunkProvider(nx::vms::client::core::ChunkProvider* value);

private:
    void setTimePeriods(const QnTimePeriodList& value);
    void updatePlayerPeriods();

public:
    MotionPlaybackMaskWatcher* const q;
    bool activeWatcher = false;
    QnTimePeriodList periods;
    nx::vms::client::core::MediaPlayer* player = nullptr;
    nx::vms::client::core::ChunkProvider* provider = nullptr;
};

MotionPlaybackMaskWatcher::Private::Private(MotionPlaybackMaskWatcher* q):
    q(q)
{
}

bool MotionPlaybackMaskWatcher::Private::active() const
{
    return activeWatcher;
}

void MotionPlaybackMaskWatcher::Private::setActive(bool value)
{
    if (activeWatcher == value)
        return;

    activeWatcher = value;
    updatePlayerPeriods();
}

nx::vms::client::core::MediaPlayer* MotionPlaybackMaskWatcher::Private::mediaPlayer()
{
    return player;
}

void MotionPlaybackMaskWatcher::Private::setMediaPlayer(nx::vms::client::core::MediaPlayer* value)
{
    if (player == value)
        return;

    player = value;
    emit q->mediaPlayerChanged();

    updatePlayerPeriods();
}

nx::vms::client::core::ChunkProvider* MotionPlaybackMaskWatcher::Private::chunkProvider() const
{
    return provider;
}

void MotionPlaybackMaskWatcher::Private::setChunkProvider(
    nx::vms::client::core::ChunkProvider* value)
{
    if (provider == value)
        return;

    if (provider)
        provider->disconnect(this);

    provider = value;
    emit q->chunkProviderChanged();

    if (!provider)
    {
        setTimePeriods(QnTimePeriodList());
        return;
    }

    const auto updatePeriods =
        [this](Qn::TimePeriodContent type)
        {
            if (type == Qn::MotionContent)
                setTimePeriods(provider->periods(Qn::MotionContent));
        };
    connect(provider, &nx::vms::client::core::ChunkProvider::periodsUpdated, this, updatePeriods);
    updatePeriods(Qn::MotionContent);
}

void MotionPlaybackMaskWatcher::Private::setTimePeriods(const QnTimePeriodList& value)
{
    if (periods == value)
        return;

    periods = value;

    updatePlayerPeriods();
}

void MotionPlaybackMaskWatcher::Private::updatePlayerPeriods()
{
    if (player)
        player->setPlaybackMask(active() ? periods : QnTimePeriodList());
}

//--------------------------------------------------------------------------------------------------

void MotionPlaybackMaskWatcher::registerQmlType()
{
    qmlRegisterType<nx::client::mobile::MotionPlaybackMaskWatcher>(
        "nx.client.mobile", 1, 0, "MotionPlaybackMaskWatcher");
}

MotionPlaybackMaskWatcher::MotionPlaybackMaskWatcher(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

MotionPlaybackMaskWatcher::~MotionPlaybackMaskWatcher()
{
}

bool MotionPlaybackMaskWatcher::active() const
{
    return d->active();
}

void MotionPlaybackMaskWatcher::setActive(bool value)
{
    d->setActive(value);
}

nx::vms::client::core::MediaPlayer* MotionPlaybackMaskWatcher::mediaPlayer() const
{
    return d->mediaPlayer();
}

void MotionPlaybackMaskWatcher::setMediaPlayer(nx::vms::client::core::MediaPlayer* player)
{
    d->setMediaPlayer(player);
}

nx::vms::client::core::ChunkProvider* MotionPlaybackMaskWatcher::chunkProvider() const
{
    return d->chunkProvider();
}

void MotionPlaybackMaskWatcher::setChunkProvider(nx::vms::client::core::ChunkProvider* provider)
{
    d->setChunkProvider(provider);
}

} // namespace nx::client::mobile
