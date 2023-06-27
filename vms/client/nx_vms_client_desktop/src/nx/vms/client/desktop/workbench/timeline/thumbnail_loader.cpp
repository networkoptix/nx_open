// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_loader.h"

#include <nx/utils/math/math.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/client/core/application_context.h>
#include <recording/time_period.h>
#include <utils/common/long_runable_cleanup.h>
#include <utils/common/synctime.h>

using namespace std::literals::chrono_literals;

namespace {

static constexpr auto kMaxThumbnailCacheSizeBytes = 64 * 1024 * 1024;

int64_t periodIndex(std::chrono::milliseconds timePoint, std::chrono::milliseconds timeStep)
{
    if (!NX_ASSERT(timeStep != 0ms))
        return -1;

    return (timePoint.count() + timeStep.count() / 2) / timeStep.count();
}

std::pair<int64_t, int64_t> indexRange(
    const QnTimePeriod& timePeriod, std::chrono::milliseconds timeStep)
{
    if (!NX_ASSERT(timeStep != 0ms))
        return {};

    return std::make_pair(
        periodIndex(timePeriod.startTime(), timeStep),
        periodIndex(timePeriod.endTime(), timeStep));
}

QnTimePeriod timePeriodByIndex(int64_t periodIndex, std::chrono::milliseconds timeStep)
{
    return QnTimePeriod(timeStep * periodIndex - timeStep / 2, timeStep);
}

std::chrono::milliseconds timePeriodMidTime(const QnTimePeriod& timePeriod)
{
    return timePeriod.startTime() + timePeriod.duration() / 2;
}

class ArchiveFrameExtractorAsyncRelease: public QnLongRunnable
{
public:
    ArchiveFrameExtractorAsyncRelease(
        std::unique_ptr<nx::vms::client::desktop::ArchiveFrameExtractor> frameExtractor)
        :
        m_frameExtractor(std::move(frameExtractor))
    {
    }

    virtual void run() override
    {
        m_frameExtractor.reset();
    }

private:
    std::unique_ptr<nx::vms::client::desktop::ArchiveFrameExtractor> m_frameExtractor;
};

} // namespace

namespace nx::vms::client::desktop::workbench::timeline {

ThumbnailLoader::ThumbnailLoader(const QnMediaResourcePtr& resource, Mode mode):
    m_mode(mode),
    m_frameExtractor(new ArchiveFrameExtractor(
        resource,
        ArchiveFrameExtractor::VideoQuality::low))
{
    connect(m_frameExtractor.get(), &ArchiveFrameExtractor::frameRequestDone,
        this, &ThumbnailLoader::handleExtractedFrame);
}

ThumbnailLoader::~ThumbnailLoader()
{
    m_frameExtractor->disconnect(this);

    auto frameExtractorAsyncRelease =
        std::make_unique<ArchiveFrameExtractorAsyncRelease>(std::move(m_frameExtractor));
    frameExtractorAsyncRelease->start();

    nx::vms::common::appContext()->longRunableCleanup()->cleanupAsync(
        std::move(frameExtractorAsyncRelease));
}

std::chrono::milliseconds ThumbnailLoader::timeStep() const
{
    return m_timeStep;
}

void ThumbnailLoader::setTimeStep(std::chrono::milliseconds timeStep)
{
    if (m_timeStep == timeStep)
        return;

    m_timeStep = timeStep;
    invalidateThumbnails();
}

ThumbnailLoader::TransformParams ThumbnailLoader::transformParams() const
{
    return m_transformParams;
}

void ThumbnailLoader::setTransformParams(const TransformParams& params)
{
    if (m_transformParams.size == params.size
        && qFuzzyEquals(m_transformParams.rotation, params.rotation))
    {
        return;
    }

    m_transformParams = params;
    invalidateThumbnails();

    emit scalingTargetSizeChanged(QnAspectRatio::isRotated90(m_transformParams.rotation)
        ? m_transformParams.size.transposed()
        : m_transformParams.size);
}

QnTimePeriod ThumbnailLoader::timeWindow() const
{
    return m_timeWindow;
}

void ThumbnailLoader::setTimeWindow(const QnTimePeriod& newTimeWindow)
{
    if (newTimeWindow == m_timeWindow)
        return;

    const auto needProcess = m_timeStep != 0ms
        && !newTimeWindow.isNull()
        && indexRange(newTimeWindow, m_timeStep) != indexRange(m_timeWindow, m_timeStep);

    m_timeWindow = newTimeWindow;

    if (needProcess)
        process();
}

QVector<ThumbnailPtr> ThumbnailLoader::thumbnails() const
{
    return QVector<ThumbnailPtr>(
        m_thumbnailsByPeriodIndex.cbegin(), m_thumbnailsByPeriodIndex.cend());
}

ThumbnailPtr ThumbnailLoader::thumbnailAt(
    std::chrono::milliseconds desiredTime,
    std::chrono::milliseconds* thumbnailTime)
{
    if (m_timeStep == 0ms)
        return ThumbnailPtr();

    auto index = periodIndex(desiredTime, m_timeStep);
    auto timePeriod = timePeriodByIndex(index, m_timeStep);

    if (thumbnailTime)
        *thumbnailTime = timePeriodMidTime(timePeriod);

    if (m_thumbnailsByPeriodIndex.contains(index))
        return m_thumbnailsByPeriodIndex.value(index);

    setTimeWindow(timePeriod);

    return {};
}

void ThumbnailLoader::invalidateThumbnails()
{
    m_frameExtractor->clearRequestQueue();
    m_frameExtractor->setScaleSize(m_transformParams.size);
    m_processedPeriodsIndexes.clear();
    m_thumbnailsByPeriodIndex.clear();
    m_timeWindow.clear();
    ++m_generation;

    process();

    emit thumbnailsInvalidated();
}

void ThumbnailLoader::process()
{
    if (m_timeStep == 0ms || m_timeWindow.isNull())
        return;

    auto [startPeriodIndex, endPeriodIndex] = indexRange(m_timeWindow, m_timeStep);

    for (int64_t index = startPeriodIndex; index < endPeriodIndex; ++index)
    {
        if (!m_processedPeriodsIndexes.contains(index))
        {
            auto period = timePeriodByIndex(index, m_timeStep);
            if (m_recordedTimePeriods.has_value() && !m_recordedTimePeriods->intersects(period))
                continue;

            auto time = period.startTime() + (m_timeStep) / 2;
            auto tolerance = m_timeStep / 2;

            if (time.count() < 0)
                continue;

            if (time.count() >= qnSyncTime->currentMSecsSinceEpoch())
                continue;

            m_processedPeriodsIndexes.insert(index);
            m_frameExtractor->requestFrame({time, tolerance, m_generation});
        }
    }
}

void ThumbnailLoader::addThumbnail(const ThumbnailPtr& thumbnail)
{
    if (m_timeStep == 0ms)
        return;

    const auto index = periodIndex(thumbnail->time(), m_timeStep);
    if (!m_thumbnailsByPeriodIndex.contains(index))
    {
        m_thumbnailsByPeriodIndex.insert(index, thumbnail);
        emit thumbnailLoaded(thumbnail);
    }
}

void ThumbnailLoader::handleExtractedFrame(ArchiveFrameExtractor::Result reply)
{
    if (reply.request.userData.toInt() != m_generation)
        return;

    if (!reply.decodedFrame)
        return;

    const auto actualTime = std::chrono::round<std::chrono::milliseconds>(
        std::chrono::microseconds(reply.decodedFrame->pkt_dts));

    auto image = reply.decodedFrame->toImage();
    if (!qFuzzyIsNull(m_transformParams.rotation))
    {
        QTransform matrix;
        matrix.rotate(m_transformParams.rotation);
        image = image.transformed(matrix);
    }

    const ThumbnailPtr thumbnail(new Thumbnail(
        image, image.size(), reply.request.timePoint, actualTime, m_generation));

    addThumbnail(thumbnail);

    if (!m_thumbnailsByPeriodIndex.empty())
    {
        const auto thumbnailCacheSizeBytes = m_thumbnailsByPeriodIndex.size()
            * m_thumbnailsByPeriodIndex.begin().value()->image().sizeInBytes();
        if (thumbnailCacheSizeBytes > kMaxThumbnailCacheSizeBytes)
            invalidateThumbnails();
    }
}

void ThumbnailLoader::setRecordedTimePeriods(const QnTimePeriodList& recordedTimePeriods)
{
    if (m_recordedTimePeriods.has_value() && m_recordedTimePeriods.value() == recordedTimePeriods)
        return;

    m_recordedTimePeriods = recordedTimePeriods;
    process();
}

} // namespace nx::vms::client::desktop::workbench::timeline
