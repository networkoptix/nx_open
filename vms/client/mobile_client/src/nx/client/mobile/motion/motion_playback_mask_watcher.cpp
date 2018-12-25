#include "motion_playback_mask_watcher.h"

#include <QtQml/QtQml>

#include <camera/camera_chunk_provider.h>
#include <nx/client/core/media/media_player.h>

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

    QnCameraChunkProvider* chunkProvider() const;
    void setChunkProvider(QnCameraChunkProvider* value);

private:
    void setTimePeriods(const QnTimePeriodList& value);
    void updatePlayerPeriods();

public:
    MotionPlaybackMaskWatcher* const q;
    bool activeWatcher = false;
    QnTimePeriodList periods;
    nx::vms::client::core::MediaPlayer* player = nullptr;
    QnCameraChunkProvider* provider = nullptr;
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

QnCameraChunkProvider* MotionPlaybackMaskWatcher::Private::chunkProvider() const
{
    return provider;
}

void MotionPlaybackMaskWatcher::Private::setChunkProvider(QnCameraChunkProvider* value)
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

    const auto updatePeriods = [this]() { setTimePeriods(provider->timePeriods(Qn::MotionContent)); };
    connect(provider, &QnCameraChunkProvider::timePeriodsUpdated, this, updatePeriods);
    updatePeriods();
}

void MotionPlaybackMaskWatcher::Private::setTimePeriods(const QnTimePeriodList& value)
{
    QnTimePeriodList target;
    if (value.isEmpty())
        target.append(QnTimePeriod(0, 0)); //< Motion mask with empty period to support jump to live.
    else
        target = value;

    if (periods == target)
        return;

    periods = target;

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

QnCameraChunkProvider* MotionPlaybackMaskWatcher::chunkProvider() const
{
    return d->chunkProvider();
}

void MotionPlaybackMaskWatcher::setChunkProvider(QnCameraChunkProvider* provider)
{
    d->setChunkProvider(provider);
}

} // namespace nx::client::mobile
