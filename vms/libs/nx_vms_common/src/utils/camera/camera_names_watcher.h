// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>

#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context_aware.h>

namespace utils {

class NX_VMS_COMMON_API QnCameraNamesWatcher:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

    typedef QObject base_type;

public:
    explicit QnCameraNamesWatcher(nx::vms::common::SystemContext* systemContext);
    ~QnCameraNamesWatcher();
    QString getCameraName(const QnUuid& cameraId);

signals:
    void cameraNameChanged(const QnUuid& cameraId);

public:
    QHash<QnUuid, QString> m_names;
};

} // namespace utils
