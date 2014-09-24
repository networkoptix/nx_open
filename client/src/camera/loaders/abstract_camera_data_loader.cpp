#include "abstract_camera_data_loader.h"

#include <camera/data/abstract_camera_data.h>

QnAbstractCameraDataLoader::QnAbstractCameraDataLoader(const QnResourcePtr &resource, const Qn::CameraDataType dataType, QObject *parent): 
    QObject(parent),
    m_resource(resource),
    m_dataType(dataType)
{
    connect(this, &QnAbstractCameraDataLoader::delayedReady, this, &QnAbstractCameraDataLoader::ready, Qt::QueuedConnection);
}

QnAbstractCameraDataLoader::~QnAbstractCameraDataLoader() {
}

void QnAbstractCameraDataLoader::discardCachedData(const qint64 resolutionMs /*= 0*/) {
    Q_UNUSED(resolutionMs) 
}