#include "camera_chunk_provider.h"

#include "camera/loaders/flat_camera_data_loader.h"
#include "camera/data/abstract_camera_data.h"
#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/camera_history.h"

QnCameraChunkProvider::QnCameraChunkProvider(QObject *parent) :
    QObject(parent),
    m_loader(nullptr)
{
}

QnTimePeriodList QnCameraChunkProvider::timePeriods() const {
    return m_periodList;
}

QString QnCameraChunkProvider::resourceId() const {
    if (!m_loader)
        return QString();

    return m_loader->resource()->getId().toString();
}

void QnCameraChunkProvider::setResourceId(const QString &id) {
    QString oldId = resourceId();
    if (id == oldId)
        return;

    if (m_loader) {
        disconnect(m_loader, nullptr, this, nullptr);
        disconnect(qnCameraHistoryPool, nullptr, m_loader, nullptr);
        m_loader->deleteLater();
        m_loader = nullptr;
    }

    m_periodList.clear();
    emit timePeriodsUpdated();
    emit bottomBoundChanged();

    QnVirtualCameraResourcePtr camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return;

    m_loader = new QnFlatCameraDataLoader(camera, Qn::RecordedTimePeriod, this);
    connect(m_loader, &QnFlatCameraDataLoader::ready, this, [this](const QnAbstractCameraDataPtr &data) {
        m_periodList = data->dataSource();
        emit timePeriodsUpdated();
        emit bottomBoundChanged();
    });

    connect(qnCameraHistoryPool, &QnCameraHistoryPool::cameraFootageChanged, m_loader, [this](){ m_loader->discardCachedData(); } );
    connect(qnCameraHistoryPool, &QnCameraHistoryPool::cameraHistoryInvalidated, this, &QnCameraChunkProvider::update);

}

QDateTime QnCameraChunkProvider::bottomBound() const {
    QnTimePeriod boundingPeriod = m_periodList.boundingPeriod();
    if (boundingPeriod.startTimeMs > 0)
        return QDateTime::fromMSecsSinceEpoch(boundingPeriod.startTimeMs, Qt::UTC);
    else
        return QDateTime();
}

void QnCameraChunkProvider::update() {
    m_loader->load(QString(), 1);
}

