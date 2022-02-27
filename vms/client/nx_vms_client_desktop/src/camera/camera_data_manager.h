// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <common/common_module_aware.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

class QnCachingCameraDataLoader;
typedef QSharedPointer<QnCachingCameraDataLoader> QnCachingCameraDataLoaderPtr;

class QnCameraDataManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
public:
    explicit QnCameraDataManager(QnCommonModule* commonModule, QObject* parent = nullptr);
    virtual ~QnCameraDataManager();

    QnCachingCameraDataLoaderPtr loader(const QnMediaResourcePtr &resource, bool createIfNotExists = true);

    void clearCache();

signals:
    void periodsChanged(const QnMediaResourcePtr &resource, Qn::TimePeriodContent type, qint64 startTimeMs);

private:
    mutable QHash<QnMediaResourcePtr, QnCachingCameraDataLoaderPtr> m_loaderByResource;
};
