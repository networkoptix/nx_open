// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QObject>

#include <core/resource_management/resource_pool.h>
#include <nx/utils/uuid.h>

class NX_VMS_COMMON_API QnCameraNamesWatcher:
    public QObject
{
    Q_OBJECT

    typedef QObject base_type;

public:
    explicit QnCameraNamesWatcher(QnResourcePool* resourcePool);
    ~QnCameraNamesWatcher();
    QString getCameraName(const nx::Uuid& cameraId);

signals:
    void cameraNameChanged(const nx::Uuid& cameraId);

public:
    QHash<nx::Uuid, QString> m_names;

private:
    QnResourcePool* const m_resourcePool;
};
