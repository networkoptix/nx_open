// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/ptz_controller_pool.h>

namespace nx::vms::client::core {

/**
 * Simple and minimalistic version of the PTZ controller pool. Initializes controllers only after
 * they were manually requested. Does not listen to any camera and server signals. Does not process
 * server status changes.
 */
class NX_VMS_CLIENT_CORE_API CrossSystemPtzControllerPool: public QnPtzControllerPool
{
    Q_OBJECT
    using base_type = QnPtzControllerPool;

public:
    CrossSystemPtzControllerPool(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);

    QnPtzControllerPtr ensureControllerExists(const QnResourcePtr& resource);

protected:
    virtual QnPtzControllerPtr createController(const QnResourcePtr& resource) const override;

};

} // namespace nx::vms::client::core
