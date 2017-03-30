#include "camera_chunk_provider.h"

#include <common/common_module.h>

#include <camera/loaders/flat_camera_data_loader.h>
#include <camera/data/abstract_camera_data.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>

QnCameraChunkProvider::QnCameraChunkProvider(QObject* parent):
    QObject(parent)
{
}

QnTimePeriodList QnCameraChunkProvider::timePeriods() const
{
    return m_periodList;
}

QString QnCameraChunkProvider::resourceId() const
{
    if (!m_loader)
        return QString();

    return m_loader->resource()->getId().toString();
}

void QnCameraChunkProvider::setResourceId(const QString& id)
{
    QString oldId = resourceId();
    if (id == oldId)
        return;

    if (m_loader)
    {
        m_loader->disconnect(this);
        cameraHistoryPool()->disconnect(m_loader);
        m_loader->deleteLater();
        m_loader = nullptr;
    }

    m_periodList.clear();
    emit timePeriodsUpdated();
    emit bottomBoundChanged();
    emit bottomBoundDateChanged();

    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(QnUuid(id));

    m_loading = !camera.isNull();
    emit loadingChanged();

    if (!camera)
        return;

    m_loader = new QnFlatCameraDataLoader(
        camera,
        commonModule()->currentServer(),
        Qn::RecordingContent,
        this);
    connect(m_loader, &QnFlatCameraDataLoader::ready, this,
        [this](const QnAbstractCameraDataPtr& data)
        {
            m_periodList = data->dataSource();
            emit timePeriodsUpdated();
            emit bottomBoundChanged();
            emit bottomBoundDateChanged();

            if (m_loading)
            {
                m_loading = false;
                emit loadingChanged();
            }
        });

    connect(cameraHistoryPool(), &QnCameraHistoryPool::cameraFootageChanged,
        m_loader, [this](){ m_loader->discardCachedData(); } );
    connect(cameraHistoryPool(), &QnCameraHistoryPool::cameraHistoryInvalidated,
        this, &QnCameraChunkProvider::update);

    update();
}

qint64 QnCameraChunkProvider::bottomBound() const
{
    const auto boundingPeriod = m_periodList.boundingPeriod();
    return boundingPeriod.startTimeMs > 0 ? boundingPeriod.startTimeMs : -1;
}

QDateTime QnCameraChunkProvider::bottomBoundDate() const
{
    const auto bottomBoundMs = bottomBound();
    return bottomBoundMs > 0
        ? QDateTime::fromMSecsSinceEpoch(bottomBoundMs, Qt::UTC)
        : QDateTime();
}

bool QnCameraChunkProvider::isLoading() const
{
    return m_loading;
}

QDateTime QnCameraChunkProvider::closestChunkStartDate(
    const QDateTime& dateTime, bool forward) const
{
    auto it = m_periodList.findNearestPeriod(dateTime.toMSecsSinceEpoch(), forward);
    if (it == m_periodList.end())
        return QDateTime();
    return QDateTime::fromMSecsSinceEpoch(it->startTimeMs, Qt::UTC);
}

qint64 QnCameraChunkProvider::closestChunkStartMs(qint64 position, bool forward) const
{
    auto it = m_periodList.findNearestPeriod(position, forward);
    if (it == m_periodList.end())
        return -1;
    return it->startTimeMs;
}

QDateTime QnCameraChunkProvider::closestChunkEndDate(
    const QDateTime& dateTime, bool forward) const
{
    auto it = m_periodList.findNearestPeriod(dateTime.toMSecsSinceEpoch(), forward);
    if (it == m_periodList.end())
        return QDateTime();
    return QDateTime::fromMSecsSinceEpoch(it->endTimeMs(), Qt::UTC);
}

qint64 QnCameraChunkProvider::closestChunkEndMs(qint64 position, bool forward) const
{
    auto it = m_periodList.findNearestPeriod(position, forward);
    if (it == m_periodList.end())
        return -1;
    return it->endTimeMs();
}

QDateTime QnCameraChunkProvider::nextChunkStartDate(
    const QDateTime& dateTime, bool forward) const
{
    auto it = m_periodList.findNearestPeriod(dateTime.toMSecsSinceEpoch(), forward);
    if (it == m_periodList.end() || ++it == m_periodList.end())
        return QDateTime();
    return QDateTime::fromMSecsSinceEpoch(it->startTimeMs, Qt::UTC);
}

qint64 QnCameraChunkProvider::nextChunkStartMs(qint64 position, bool forward) const
{
    auto it = m_periodList.findNearestPeriod(position, forward);
    if (it == m_periodList.end() || ++it == m_periodList.end())
        return -1;
    return it->startTimeMs;
}

void QnCameraChunkProvider::update()
{
    if (!m_loader)
        return;

    m_loader->load(QString(), 1);

    auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(
        m_loader->resource()->getId());
    cameraHistoryPool()->updateCameraHistoryAsync(camera, nullptr);
}
