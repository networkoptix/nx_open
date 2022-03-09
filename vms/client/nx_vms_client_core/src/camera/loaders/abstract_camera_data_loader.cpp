// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_camera_data_loader.h"

#include <core/resource/resource.h>
#include <recording/time_period.h>

QnAbstractCameraDataLoader::QnAbstractCameraDataLoader(
    const QnResourcePtr& resource,
    const Qn::TimePeriodContent dataType,
    QObject* parent)
    :
    QObject(parent),
    m_resource(resource),
    m_dataType(dataType)
{
}

QnAbstractCameraDataLoader::~QnAbstractCameraDataLoader()
{
}

void QnAbstractCameraDataLoader::discardCachedData(const qint64 /*resolutionMs*/)
{
}

QString QnAbstractCameraDataLoader::idForToStringFromPtr() const
{
    return nx::format("%1 for %2").args(m_dataType, m_resource->getName());
}

QnResourcePtr QnAbstractCameraDataLoader::resource() const
{
    return m_resource;
}

const QnTimePeriodList& QnAbstractCameraDataLoader::periods() const
{
    return m_loadedData;
}
