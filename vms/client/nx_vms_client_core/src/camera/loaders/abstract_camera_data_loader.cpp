#include "abstract_camera_data_loader.h"

#include <core/resource/resource.h>

#include <camera/data/abstract_camera_data.h>

#include <recording/time_period.h>

QnAbstractCameraDataLoader::QnAbstractCameraDataLoader(const QnResourcePtr &resource, const Qn::TimePeriodContent dataType, QObject *parent):
    QObject(parent),
    m_resource(resource),
    m_dataType(dataType)
{
    connect(this, &QnAbstractCameraDataLoader::delayedReady, this, &QnAbstractCameraDataLoader::ready, Qt::QueuedConnection);
}

QnAbstractCameraDataLoader::~QnAbstractCameraDataLoader() {
}

void QnAbstractCameraDataLoader::discardCachedData(const qint64 resolutionMs /* = 0*/) {
    Q_UNUSED(resolutionMs)
}

QString QnAbstractCameraDataLoader::idForToStringFromPtr() const
{
    return lm("%1 for %2").args(m_dataType, m_resource->getName());
}

QnResourcePtr QnAbstractCameraDataLoader::resource() const
{
    return m_resource;
}
