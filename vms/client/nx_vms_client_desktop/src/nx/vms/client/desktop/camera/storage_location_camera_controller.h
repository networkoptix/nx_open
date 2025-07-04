// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/types/storage_location.h>
#include <nx/vms/client/desktop/system_context_aware.h>

#include "camera_fwd.h"

namespace nx::vms::client::desktop {

/**
 * Keeps the list of all opened cameras and setups / updates chunks storage location for them.
 */
class StorageLocationCameraController: public QObject, SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    StorageLocationCameraController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~StorageLocationCameraController() override;

    nx::vms::api::StorageLocation storageLocation() const;
    void setStorageLocation(nx::vms::api::StorageLocation value);

    void registerConsumer(QnResourceDisplayPtr display);
    void unregisterConsumer(QnResourceDisplayPtr display);

private:
    void updateStorageLocationByConsumers();

    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
