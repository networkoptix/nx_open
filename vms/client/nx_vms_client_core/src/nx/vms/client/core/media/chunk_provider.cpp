// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "chunk_provider.h"

#include <QtQml/QtQml>

#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/resource/data_loaders/flat_camera_data_loader.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

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

private:
    void updateInternal();

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
};

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

const QnTimePeriodList& ChunkProvider::ChunkProviderInternal::periods() const
{
    static QnTimePeriodList kEmptyPeriods;
    return m_loader ? m_loader->periods() : kEmptyPeriods;
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
    m_providers(
        [this]()
        {
            ProvidersHash result;
            result.insert(Qn::RecordingContent,
                ChunkProviderPtr(new ChunkProviderInternal(Qn::RecordingContent, this)));
            result.insert(Qn::MotionContent,
                ChunkProviderPtr(new ChunkProviderInternal(Qn::MotionContent, this)));
            return result;
        }())
{
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

QnResource* ChunkProvider::rawResource() const
{
    return m_providers[Qn::RecordingContent]->resource().data();
}

void ChunkProvider::setRawResource(QnResource* value)
{
    QnResourcePtr resource = value ? value->toSharedPointer() : QnResourcePtr();

    for (const auto& provider: m_providers)
        provider->setResource(resource);
}

qint64 ChunkProvider::bottomBound() const
{
    const auto boundingPeriod = periods(Qn::RecordingContent).boundingPeriod();
    return boundingPeriod.startTimeMs > 0 ? boundingPeriod.startTimeMs : -1;
}

const QnTimePeriodList& ChunkProvider::periods(Qn::TimePeriodContent type) const
{
    return m_providers[type]->periods();
}

bool ChunkProvider::isLoading() const
{
    return m_providers[Qn::RecordingContent]->loading();
}

bool ChunkProvider::isLoadingMotion() const
{
    return m_providers[Qn::MotionContent]->loading();
}

void ChunkProvider::handleLoadingChanged(Qn::TimePeriodContent contentType)
{
    if (contentType == Qn::RecordingContent)
        emit loadingChanged();
    else
        emit loadingMotionChanged();
}

qint64 ChunkProvider::closestChunkEndMs(qint64 position, bool forward) const
{
    const auto data = periods(Qn::RecordingContent);
    auto it = data.findNearestPeriod(position, forward);
    return it == data.end() ? -1 : it->endTimeMs();
}

void ChunkProvider::update()
{
    for (const auto& provider: m_providers)
        provider->update();
}

QString ChunkProvider::motionFilter() const
{
    return m_providers[Qn::MotionContent]->filter();
}

void ChunkProvider::setMotionFilter(const QString& value)
{
    m_providers[Qn::MotionContent]->setFilter(value);
}

} // namespace nx::vms::client::core
