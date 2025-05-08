// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/types/storage_location.h>
#include <nx/vms/client/core/resource/data_loaders/camera_data_loader_fwd.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CameraDataManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit CameraDataManager(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~CameraDataManager() override;

    CachingCameraDataLoaderPtr loader(
        const QnMediaResourcePtr& resource, bool createIfNotExists = true);

    void clearCache();

    nx::vms::api::StorageLocation storageLocation() const;
    void setStorageLocation(nx::vms::api::StorageLocation value);

signals:
    void periodsChanged(
        const QnMediaResourcePtr& resource,
        Qn::TimePeriodContent type,
        qint64 startTimeMs);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
