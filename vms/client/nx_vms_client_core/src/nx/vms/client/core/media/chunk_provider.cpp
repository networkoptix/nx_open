// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "chunk_provider.h"

#include <chrono>
#include <optional>
#include <unordered_map>

#include <QtQml/QtQml>

#include <analytics/db/analytics_db_types.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/resource/data_loaders/flat_camera_data_loader.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/synctime.h>

using namespace std::chrono;
using namespace nx::analytics::db;

namespace nx::vms::client::core {

// ------------------------------------------------------------------------------------------------

class ChunkProvider::ChunkProviderInternal
{
public:
    ChunkProviderInternal(
        Qn::TimePeriodContent contentType,
        ChunkProvider* owner);

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& value);

    const QnTimePeriodList& periods() const;

    const QString& filter() const;
    void setFilter(const QString& filter);

    bool loading() const;

    void update();

    void setForcedBottomBound(std::optional<milliseconds> value, bool notifyChange = true);
    void updateClippedPeriods();

private:
    void updateInternal();

    const QnTimePeriodList& rawPeriods() const;

    void setLoading(bool value);
    void notifyAboutTimePeriodsChange();

private:
    ChunkProvider* const q;
    const Qn::TimePeriodContent m_contentType;
    QScopedPointer<FlatCameraDataLoader> m_loader;
    QString m_filter;
    bool m_loading = false;
    int updateTriesCount = 0;
    nx::utils::ScopedConnections connections;
    std::optional<milliseconds> m_forcedBottomBound;
    QnTimePeriodList m_clippedPeriods;
};

// ------------------------------------------------------------------------------------------------

class ChunkProvider::Private: public QObject
{
    ChunkProvider* const q;

    int analyticsUpdateHandle = 0;
    QTimer analyticsUpdateTimer;

public:
    using ProvidersHash =
        std::unordered_map<Qn::TimePeriodContent, std::unique_ptr<ChunkProviderInternal>>;

    const ProvidersHash providers{
        [this]()
        {
            static const std::vector<Qn::TimePeriodContent> kContentTypes{
                Qn::RecordingContent, Qn::MotionContent, Qn::AnalyticsContent};

            ProvidersHash result;
            for (const auto type: kContentTypes)
                result.emplace(type, std::make_unique<ChunkProviderInternal>(type, q));

            return result;
        }()};

    explicit Private(ChunkProvider* q);

    // This is a workaround for our servers often having analytics chunks without analytics DB.
    // An API request is sent to find out the earliest object track the analytics DB contains.
    void updateAnalyticsBottom();
    void resetAnalyticsBottom();
};

// ------------------------------------------------------------------------------------------------

ChunkProvider::Private::Private(ChunkProvider* q): q(q)
{
    analyticsUpdateTimer.setInterval(1h);
    analyticsUpdateTimer.callOnTimeout([this]() { updateAnalyticsBottom(); });
}

void ChunkProvider::Private::updateAnalyticsBottom()
{
    const auto resource = q->resource();
    if (!resource)
        return;

    const QPointer<core::SystemContext> systemContext =
        resource->systemContext()->as<core::SystemContext>();

    const auto api = systemContext->connectedServerApi();
    if (!NX_ASSERT(api))
        return;

    Filter filter;
    filter.deviceIds.insert(resource->getId());
    filter.maxObjectTracksToSelect = 1;
    filter.sortOrder = Qt::AscendingOrder;

    const auto responseHandler = nx::utils::guarded(this,
        [this, nowUs = qnSyncTime->currentTimePoint()](
            bool success, rest::Handle handle, LookupResult&& result)
        {
            if (!success || handle != analyticsUpdateHandle)
                return;

            providers.at(Qn::AnalyticsContent)->setForcedBottomBound(duration_cast<milliseconds>(
                result.empty() ? nowUs : microseconds(result.front().firstAppearanceTimeUs)));

            analyticsUpdateHandle = 0;
        });

    analyticsUpdateHandle = api->lookupObjectTracks(filter, /*isLocal*/ false, responseHandler,
        thread());

    if (!analyticsUpdateTimer.isActive())
        analyticsUpdateTimer.start();
}

void ChunkProvider::Private::resetAnalyticsBottom()
{
    analyticsUpdateHandle = 0;
    analyticsUpdateTimer.stop();
    providers.at(Qn::AnalyticsContent)->setForcedBottomBound(std::nullopt, /*notifyChange*/ false);
}

// ------------------------------------------------------------------------------------------------

ChunkProvider::ChunkProviderInternal::ChunkProviderInternal(
    Qn::TimePeriodContent contentType,
    ChunkProvider* owner)
    :
    q(owner),
    m_contentType(contentType)
{
}

QnResourcePtr ChunkProvider::ChunkProviderInternal::resource() const
{
    return m_loader ? m_loader->resource() : QnResourcePtr();
}

void ChunkProvider::ChunkProviderInternal::setResource(const QnResourcePtr& value)
{
    if (resource() == value)
        return;

    connections.reset();
    m_loader.reset();
    notifyAboutTimePeriodsChange();
    setLoading(false);

    const auto camera = value.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    setLoading(true);
    m_loader.reset(new FlatCameraDataLoader(camera, m_contentType));

    connections << connect(m_loader.get(), &FlatCameraDataLoader::ready, q,
        [this](qint64 /*startTimeMs*/)
        {
            updateClippedPeriods();
            notifyAboutTimePeriodsChange();
            setLoading(false);
        });

    connections << connect(m_loader.get(), &FlatCameraDataLoader::failed, q,
        [this]()
        {
            if (--updateTriesCount > 0)
                updateInternal();
            else
                setLoading(false);
        });

    if (auto systemContext = SystemContext::fromResource(camera); NX_ASSERT(systemContext))
    {
        const auto historyPool = systemContext->cameraHistoryPool();
        connections << connect(historyPool, &QnCameraHistoryPool::cameraFootageChanged, q,
            [this](){ m_loader->discardCachedData(); });
        connections << connect(historyPool, &QnCameraHistoryPool::cameraHistoryInvalidated, q,
            [this]() { update(); });
    }

    update();
}

const QnTimePeriodList& ChunkProvider::ChunkProviderInternal::rawPeriods() const
{
    static QnTimePeriodList kEmptyPeriods;
    return m_loader ? m_loader->periods() : kEmptyPeriods;
}

const QnTimePeriodList& ChunkProvider::ChunkProviderInternal::periods() const
{
    static QnTimePeriodList kEmptyPeriods;
    return m_forcedBottomBound ? m_clippedPeriods : rawPeriods();
}

void ChunkProvider::ChunkProviderInternal::setForcedBottomBound(
    std::optional<milliseconds> value, bool notifyChange)
{
    if (m_forcedBottomBound == value)
        return;

    m_forcedBottomBound = value;
    updateClippedPeriods();

    if (notifyChange)
        emit q->periodsUpdated(m_contentType);
}

void ChunkProvider::ChunkProviderInternal::updateClippedPeriods()
{
    if (m_forcedBottomBound)
    {
        m_clippedPeriods = rawPeriods().intersectedPeriods(
            QnTimePeriod{m_forcedBottomBound->count(), QnTimePeriod::kInfiniteDuration});
    }
    else if (!m_clippedPeriods.empty())
    {
        m_clippedPeriods = {};
    }
}

bool ChunkProvider::ChunkProviderInternal::loading() const
{
    return m_loading;
}

const QString& ChunkProvider::ChunkProviderInternal::filter() const
{
    return m_filter;
}

void ChunkProvider::ChunkProviderInternal::setFilter(const QString& filter)
{
    if (m_filter == filter)
        return;

    m_filter = filter;
    emit q->motionFilterChanged();

    setLoading(true);
    update();
}

void ChunkProvider::ChunkProviderInternal::update()
{
    updateTriesCount = 3;
    updateInternal();
}

void ChunkProvider::ChunkProviderInternal::updateInternal()
{
    if (!m_loader)
        return;

    m_loader->load(m_filter, 1);

    if (const auto camera = m_loader->resource().dynamicCast<QnVirtualCameraResource>())
    {
        if (auto systemContext = SystemContext::fromResource(camera); NX_ASSERT(systemContext))
            systemContext->cameraHistoryPool()->updateCameraHistoryAsync(camera, nullptr);
    }
}

void ChunkProvider::ChunkProviderInternal::setLoading(bool value)
{
    if (m_loading == value)
        return;

    m_loading = value;
    q->handleLoadingChanged(m_contentType);
}

void ChunkProvider::ChunkProviderInternal::notifyAboutTimePeriodsChange()
{
    emit q->periodsUpdated(m_contentType);
    emit q->bottomBoundChanged();
}

//-------------------------------------------------------------------------------------------------

ChunkProvider::ChunkProvider(QObject* parent):
    base_type(parent),
    d(new Private{this})
{
}

ChunkProvider::~ChunkProvider()
{
    // Required here for forward-declared scoped pointer destruction.
}

bool ChunkProvider::hasChunks() const
{
    return hasPeriods(Qn::RecordingContent);
}

bool ChunkProvider::hasMotionChunks() const
{
    return hasPeriods(Qn::MotionContent);
}

void ChunkProvider::registerQmlType()
{
    qmlRegisterType<ChunkProvider>("nx.vms.client.core", 1, 0, "ChunkProvider");
}

QnResourcePtr ChunkProvider::resource() const
{
    return d->providers.at(Qn::RecordingContent)->resource();
}

QnResource* ChunkProvider::rawResource() const
{
    return resource().get();
}

void ChunkProvider::setRawResource(QnResource* value)
{
    if (value == rawResource())
        return;

    QnResourcePtr resource = value ? value->toSharedPointer() : QnResourcePtr();
    d->resetAnalyticsBottom();

    for (const auto& [_, provider]: d->providers)
        provider->setResource(resource);

    d->updateAnalyticsBottom();
    emit resourceChanged();
}

qint64 ChunkProvider::bottomBound() const
{
    const auto boundingPeriod = periods(Qn::RecordingContent).boundingPeriod();
    return boundingPeriod.startTimeMs > 0 ? boundingPeriod.startTimeMs : -1;
}

const QnTimePeriodList& ChunkProvider::periods(Qn::TimePeriodContent type) const
{
    return d->providers.at(type)->periods();
}

bool ChunkProvider::isLoading() const
{
    return d->providers.at(Qn::RecordingContent)->loading();
}

bool ChunkProvider::isLoadingMotion() const
{
    return d->providers.at(Qn::MotionContent)->loading();
}

bool ChunkProvider::isLoadingAnalytics() const
{
    return d->providers.at(Qn::AnalyticsContent)->loading();
}

void ChunkProvider::handleLoadingChanged(Qn::TimePeriodContent contentType)
{
    switch (contentType)
    {
        case Qn::RecordingContent:
            emit loadingChanged();
            break;
        case Qn::MotionContent:
            emit loadingMotionChanged();
            break;
        case Qn::AnalyticsContent:
            emit loadingAnalyticsChanged();
            break;
        default:
            break;
    }
}

qint64 ChunkProvider::closestChunkEndMs(qint64 position, bool forward) const
{
    const auto data = periods(Qn::RecordingContent);
    auto it = data.findNearestPeriod(position, forward);
    return it == data.end() ? -1 : it->endTimeMs();
}

void ChunkProvider::update()
{
    for (const auto& [_, provider]: d->providers)
        provider->update();
}

QString ChunkProvider::motionFilter() const
{
    return d->providers.at(Qn::MotionContent)->filter();
}

void ChunkProvider::setMotionFilter(const QString& value)
{
    d->providers.at(Qn::MotionContent)->setFilter(value);
}

} // namespace nx::vms::client::core
