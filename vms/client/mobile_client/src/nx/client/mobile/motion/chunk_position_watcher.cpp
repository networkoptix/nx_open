// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "chunk_position_watcher.h"

#include <QtQml/QtQml>

#include <nx/utils/datetime.h>
#include <nx/vms/client/core/media/chunk_provider.h>

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

    return useJumpTime && periods.containTime(jumpTimeMs) && jumpTimeMs <= QDateTime::currentMSecsSinceEpoch()
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
    if (it->contains(position))
    {
        if (it == periods.cbegin())
            return defaultValue(searchForward);

        --it;
    }

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
    void disconnectCurrentProvider();
    const QnTimePeriodList& periods() const;

    ChunkPositionWatcher* const q;

    qint64 position = DATETIME_NOW;
    qint64 prevChunkStartTimeMs = DATETIME_INVALID;
    qint64 nextChunkStartTimeMs = DATETIME_INVALID;

    QPointer<nx::vms::client::core::ChunkProvider> provider;

    Qn::TimePeriodContent contentType = Qn::TimePeriodContent::RecordingContent;
};

ChunkPositionWatcher::Private::Private(ChunkPositionWatcher* q):
    q(q)
{
}

void ChunkPositionWatcher::Private::updateChunksInformation()
{
    prevChunkStartTimeMs = getChunkTime(position, periods(), false);
    nextChunkStartTimeMs = getChunkTime(position, periods(), true);
    emit q->chunksChanged();
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
    connect(provider, &nx::vms::client::core::ChunkProvider::periodsUpdated,
        this, &Private::updateChunksInformation);

    updateChunksInformation();
}

const QnTimePeriodList& ChunkPositionWatcher::Private::periods() const
{
    static QnTimePeriodList kEmptyPeriods;
    return provider ? provider->periods(contentType) : kEmptyPeriods;
}

//--------------------------------------------------------------------------------------------------

ChunkPositionWatcher::ChunkPositionWatcher(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

ChunkPositionWatcher::~ChunkPositionWatcher()
{
}

Qn::TimePeriodContent ChunkPositionWatcher::contentType() const
{
    return d->contentType;
}

void ChunkPositionWatcher::setContentType(Qn::TimePeriodContent value)
{
    if (d->contentType == value)
        return;

    d->contentType = value;
    emit contentTypeChanged();

    d->updateChunksInformation();
}

qint64 ChunkPositionWatcher::position() const
{
    return d->position;
}

void ChunkPositionWatcher::setPosition(qint64 value)
{
    if (d->position == value)
        return;

    d->position = value == -1 ? DATETIME_NOW : value;
    emit positionChanged();

    d->updateChunksInformation();
}

nx::vms::client::core::ChunkProvider* ChunkPositionWatcher::chunkProvider() const
{
    return d->provider;
}

void ChunkPositionWatcher::setChunkProvider(nx::vms::client::core::ChunkProvider* value)
{
    if (d->provider == value)
        return;

    d->disconnectCurrentProvider();

    d->provider = value;
    emit chunkProviderChanged();

    d->updateProviderConnections();
}

qint64 ChunkPositionWatcher::nextChunkStartTimeMs() const
{
    return d->nextChunkStartTimeMs == DATETIME_NOW ? -1 : d->nextChunkStartTimeMs;
}

qint64 ChunkPositionWatcher::prevChunkStartTimeMs() const
{
    return d->prevChunkStartTimeMs;
}

qint64 ChunkPositionWatcher::firstChunkStartTimeMs() const
{
    const auto result = getChunkTime(0, d->periods(), true);
    return result == DATETIME_NOW ? -1 : result;
}

void ChunkPositionWatcher::registerQmlType()
{
    qmlRegisterType<nx::client::mobile::ChunkPositionWatcher>(
        "nx.client.mobile", 1, 0, "ChunkPositionWatcher");
}

} // namespace mobile
} // namespace client
} // namespace nx
