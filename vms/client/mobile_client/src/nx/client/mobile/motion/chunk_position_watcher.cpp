#include "chunk_position_watcher.h"

#include <QtQml/QtQml>

#include <camera/camera_chunk_provider.h>
#include <nx/utils/datetime.h>

namespace {

qint64 chooseNearestTimeMs(
    qint64 currentTimeMs,
    qint64 targetTimeMs,
    const QnTimePeriodList& periods)
{
    if (currentTimeMs == DATETIME_NOW || currentTimeMs == DATETIME_INVALID)
        return targetTimeMs;

    static const qint64 kMaxJumpLength = std::chrono::milliseconds(std::chrono::hours(1)).count();
    const bool searchForward = targetTimeMs > currentTimeMs;
    const auto jumpTimeMs = searchForward
        ? currentTimeMs + kMaxJumpLength
        : currentTimeMs - kMaxJumpLength;

    const bool useJumpTime = searchForward
        ? jumpTimeMs < targetTimeMs
        : jumpTimeMs > targetTimeMs;

    return useJumpTime && periods.containTime(jumpTimeMs)
        ? jumpTimeMs
        : targetTimeMs;
}

qint64 getChunkTime(
    qint64 position,
    const QnTimePeriodList& periods,
    bool searchForward)
{
    static const auto defaultValue =
        [](bool searchForward) { return searchForward ? DATETIME_NOW : DATETIME_INVALID; };

    if (periods.isEmpty())
        defaultValue(searchForward);

    auto it = periods.findNearestPeriod(position, searchForward);
    if (it == periods.cend())
        return defaultValue(searchForward);

    if (searchForward)
    {
        if (it->contains(position)) //< Here we have current chunk.
            ++it;

        const qint64 targetTimeMs = it == periods.cend()
            ? DATETIME_NOW
            : it->startTimeMs;
        return chooseNearestTimeMs(position, targetTimeMs, periods);
    }

    // Search backward.
    if (it->contains(position) && it != periods.cbegin())
        --it;

    return chooseNearestTimeMs(position, it->startTimeMs, periods);
}

} // namespace

namespace nx {
namespace client {
namespace mobile {

struct ChunkPositionWatcher::Private: public QObject
{
    Private(ChunkPositionWatcher* q);
    void updateChunksInformation();
    void updateProviderConnections();
    void updateTimePeriods();
    void disconnectCurrentProvider();

    ChunkPositionWatcher* const q;

    qint64 position = DATETIME_NOW;
    qint64 prevChunkStartTimeMs = DATETIME_INVALID;
    qint64 nextChunkStartTimeMs = DATETIME_INVALID;

    QPointer<QnCameraChunkProvider> provider;
    QnTimePeriodList periods;

    bool motionSearchMode = false;
};

ChunkPositionWatcher::Private::Private(ChunkPositionWatcher* q):
    q(q)
{
}

void ChunkPositionWatcher::Private::updateChunksInformation()
{
    prevChunkStartTimeMs = getChunkTime(position, periods, false);
    nextChunkStartTimeMs = getChunkTime(position, periods, true);
}

void ChunkPositionWatcher::Private::disconnectCurrentProvider()
{
    if (provider)
        provider->disconnect(this);
}

void ChunkPositionWatcher::Private::updateProviderConnections()
{
    if (!provider)
        return;

    disconnectCurrentProvider();

    connect(provider, &QnCameraChunkProvider::loadingMotionChanged, this,
        [this]()
        {
            if (!provider->isLoadingMotion() && motionSearchMode)
                updateTimePeriods();
        });

    connect(provider, &QnCameraChunkProvider::loadingChanged, this,
        [this]()
        {
            if (!provider->isLoading() && !motionSearchMode)
                updateTimePeriods();
        });

    updateTimePeriods();
}

void ChunkPositionWatcher::Private::updateTimePeriods()
{
    periods = provider
        ? provider->timePeriods(motionSearchMode ? Qn::MotionContent : Qn::RecordingContent)
        : QnTimePeriodList();
    periods.detach();
    updateChunksInformation();
}

//--------------------------------------------------------------------------------------------------

ChunkPositionWatcher::ChunkPositionWatcher(QObject* parent):
    d(new Private(this))
{
    connect(this, &ChunkPositionWatcher::motionSearchModeChanged,
        d, &Private::updateTimePeriods);
    connect(this, &ChunkPositionWatcher::chunkProviderChanged,
        d, &Private::updateProviderConnections);
    connect(this, &ChunkPositionWatcher::positionChanged,
        d.data(), &Private::updateChunksInformation);
}

ChunkPositionWatcher::~ChunkPositionWatcher()
{
}

bool ChunkPositionWatcher::motionSearchMode() const
{
    return d->motionSearchMode;
}

void ChunkPositionWatcher::setMotionSearchMode(bool value)
{
    if (d->motionSearchMode == value)
        return;

    d->motionSearchMode = value;
    emit motionSearchModeChanged();
}

qint64 ChunkPositionWatcher::position() const
{
    return d->position;
}

void ChunkPositionWatcher::setPosition(qint64 value)
{
    if (d->position == value)
        return;

    d->position = value;
    emit positionChanged();
}

QnCameraChunkProvider* ChunkPositionWatcher::chunkProvider() const
{
    return d->provider;
}

void ChunkPositionWatcher::setChunkProvider(QnCameraChunkProvider* value)
{
    if (d->provider == value)
        return;

    d->disconnectCurrentProvider();

    d->provider = value;
    emit chunkProviderChanged();
}

qint64 ChunkPositionWatcher::nextChunkStartTimeMs() const
{
    return d->nextChunkStartTimeMs == DATETIME_NOW ? -1 : d->nextChunkStartTimeMs;
}

qint64 ChunkPositionWatcher::prevChunkStartTimeMs() const
{
    return d->prevChunkStartTimeMs;
}

void ChunkPositionWatcher::registerQmlType()
{
    qmlRegisterType<nx::client::mobile::ChunkPositionWatcher>(
        "nx.client.mobile", 1, 0, "ChunkPositionWatcher");
}

} // namespace mobile
} // namespace client
} // namespace nx

