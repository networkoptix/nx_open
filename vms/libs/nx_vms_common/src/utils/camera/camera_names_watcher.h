// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context_aware.h>

class NX_VMS_COMMON_API QnCameraNamesWatcher:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

    typedef QObject base_type;

public:
    explicit QnCameraNamesWatcher(nx::vms::common::SystemContext* systemContext);
    ~QnCameraNamesWatcher();
    QString getCameraName(const nx::Uuid& cameraId);

signals:
    void cameraNameChanged(const nx::Uuid& cameraId);

public:
    QHash<nx::Uuid, QString> m_names;
};
