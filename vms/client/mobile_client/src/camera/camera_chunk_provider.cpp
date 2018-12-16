#include "camera_chunk_provider.h"

#include <common/common_module.h>

#include <camera/loaders/flat_camera_data_loader.h>
#include <camera/data/abstract_camera_data.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>

class QnCameraChunkProvider::ChunkProviderInternal: public Connective<QObject>
{
public:
    ChunkProviderInternal(
        Qn::TimePeriodContent contentType,
        QnCameraChunkProvider* owner);

    QString resourceId() const;
    void setResourceId(const QString& id);

    QDateTime bottomBoundDate() const;
    qint64 bottomBound() const;

    const QString& filter() const;
    void setFilter(const QString& filter);

    bool loading() const;

    qint64 closestChunkEndMs(qint64 position, bool forward) const;
    void update();

    const QnTimePeriodList& periodList() const;

private:
    void updateInternal();

    void cleanLoader();
    void setLoading(bool value);
    void setTimePeriods(const QnTimePeriodList& periods);
    QnVirtualCameraResourcePtr getCamera(const QnUuid& id);

private:
    QnCameraChunkProvider* const q;
    const Qn::TimePeriodContent m_contentType;
    QScopedPointer<QnFlatCameraDataLoader> m_loader;
    QnTimePeriodList m_periodList;
    QString m_filter;
    bool m_loading = false;
    int updateTriesCount = 0;
};

QnCameraChunkProvider::ChunkProviderInternal::ChunkProviderInternal(
    Qn::TimePeriodContent contentType,
    QnCameraChunkProvider* owner)
    :
    q(owner),
    m_contentType(contentType)
{
}

QString QnCameraChunkProvider::ChunkProviderInternal::resourceId() const
{
    return m_loader ? m_loader->resource()->getId().toString() : QString();
}

void QnCameraChunkProvider::ChunkProviderInternal::cleanLoader()
{
    if (!m_loader)
        return;

    m_loader->disconnect(this);
    q->cameraHistoryPool()->disconnect(m_loader.data());
    m_loader.reset();
}

void QnCameraChunkProvider::ChunkProviderInternal::setResourceId(const QString& id)
{
    if (id == resourceId())
        return;

    cleanLoader();
    setTimePeriods(QnTimePeriodList());

    const auto camera = getCamera(QnUuid::fromStringSafe(id));
    if (!camera)
        return;

    setLoading(true);
    const auto server = q->commonModule()->currentServer();
    m_loader.reset(new QnFlatCameraDataLoader(camera, server, m_contentType));

    connect(m_loader, &QnFlatCameraDataLoader::ready, this,
        [this](const QnAbstractCameraDataPtr& data) { setTimePeriods(data->dataSource()); });
    connect(m_loader, &QnFlatCameraDataLoader::failed, this,
        [this]()
        {
            if (--updateTriesCount > 0)
                updateInternal();
            else
                setLoading(false);
        });

    const auto historyPool = q->cameraHistoryPool();
    connect(historyPool, &QnCameraHistoryPool::cameraFootageChanged, m_loader,
        [this](){ m_loader->discardCachedData(); });
    connect(historyPool, &QnCameraHistoryPool::cameraHistoryInvalidated, this,
        [this]() { update(); });

    update();
}

qint64 QnCameraChunkProvider::ChunkProviderInternal::bottomBound() const
{
    const auto boundingPeriod = m_periodList.boundingPeriod();
    return boundingPeriod.startTimeMs > 0 ? boundingPeriod.startTimeMs : -1;
}

QDateTime QnCameraChunkProvider::ChunkProviderInternal::bottomBoundDate() const
{
    const auto bottomBoundMs = bottomBound();
    return bottomBoundMs > 0
        ? QDateTime::fromMSecsSinceEpoch(bottomBoundMs, Qt::UTC)
        : QDateTime();
}

bool QnCameraChunkProvider::ChunkProviderInternal::loading() const
{
    return m_loading;
}

qint64 QnCameraChunkProvider::ChunkProviderInternal::closestChunkEndMs(qint64 position, bool forward) const
{
    auto it = m_periodList.findNearestPeriod(position, forward);
    if (it == m_periodList.end())
        return -1;
    return it->endTimeMs();
}

const QString& QnCameraChunkProvider::ChunkProviderInternal::filter() const
{
    return m_filter;
}

void QnCameraChunkProvider::ChunkProviderInternal::setFilter(const QString& filter)
{
    if (m_filter == filter)
        return;

    m_filter = filter;
    emit q->motionFilterChanged();

    setLoading(true);
    update();
}

void QnCameraChunkProvider::ChunkProviderInternal::update()
{
    updateTriesCount = 3;
    updateInternal();
}

void QnCameraChunkProvider::ChunkProviderInternal::updateInternal()
{
    if (!m_loader)
        return;

    m_loader->load(m_filter, 1);

    const auto camera = getCamera(m_loader->resource()->getId());
    q->cameraHistoryPool()->updateCameraHistoryAsync(camera, nullptr);
}

const QnTimePeriodList& QnCameraChunkProvider::ChunkProviderInternal::periodList() const
{
    return m_periodList;
}

void QnCameraChunkProvider::ChunkProviderInternal::setLoading(bool value)
{
    if (m_loading == value)
        return;

    m_loading = value;
    q->handleLoadingChanged(m_contentType);
}

void QnCameraChunkProvider::ChunkProviderInternal::setTimePeriods(const QnTimePeriodList& periods)
{
    m_periodList = periods;

    emit q->timePeriodsUpdated();
    emit q->bottomBoundChanged();
    emit q->bottomBoundDateChanged();

    setLoading(false);
}

QnVirtualCameraResourcePtr QnCameraChunkProvider::ChunkProviderInternal::getCamera(const QnUuid& id)
{
    return q->resourcePool()->getResourceById<QnVirtualCameraResource>(id);
}

//--------------------------------------------------------------------------------------------------

QnCameraChunkProvider::QnCameraChunkProvider(QObject* parent):
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

QnTimePeriodList QnCameraChunkProvider::timePeriods(Qn::TimePeriodContent contentType) const
{
    return m_providers[contentType]->periodList();
}

bool QnCameraChunkProvider::hasChunks() const
{
    return m_providers[Qn::RecordingContent]->periodList().size();
}

bool QnCameraChunkProvider::hasMotionChunks() const
{
    return m_providers[Qn::MotionContent]->periodList().size();
}

QString QnCameraChunkProvider::resourceId() const
{
    return m_providers[Qn::RecordingContent]->resourceId();
}

void QnCameraChunkProvider::setResourceId(const QString& id)
{
    for (const auto& provider: m_providers)
        provider->setResourceId(id);
}

qint64 QnCameraChunkProvider::bottomBound() const
{
    return m_providers[Qn::RecordingContent]->bottomBound();
}

QDateTime QnCameraChunkProvider::bottomBoundDate() const
{
    return m_providers[Qn::RecordingContent]->bottomBoundDate();
}

bool QnCameraChunkProvider::isLoading() const
{
    return m_providers[Qn::RecordingContent]->loading();
}

bool QnCameraChunkProvider::isLoadingMotion() const
{
    return m_providers[Qn::MotionContent]->loading();
}

void QnCameraChunkProvider::handleLoadingChanged(Qn::TimePeriodContent contentType)
{
    if (contentType == Qn::RecordingContent)
        emit loadingChanged();
    else
        emit loadingMotionChanged();
}

qint64 QnCameraChunkProvider::closestChunkEndMs(qint64 position, bool forward) const
{
    return m_providers[Qn::RecordingContent]->closestChunkEndMs(position, forward);
}

void QnCameraChunkProvider::update()
{
    for (const auto& provider: m_providers)
        provider->update();
}

QString QnCameraChunkProvider::motionFilter() const
{
    return m_providers[Qn::MotionContent]->filter();
}

void QnCameraChunkProvider::setMotionFilter(const QString& value)
{
    m_providers[Qn::MotionContent]->setFilter(value);
}
