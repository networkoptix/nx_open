// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Watches the resource pool for layouts that contain local resources that are not in resource pool
 * yet. Once such resource is inserted into the pool, its children resources are initialized and
 * inserted into the pool.
 */
class LocalResourcesInitializer: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    LocalResourcesInitializer(SystemContext* systemContext, QObject* parent = nullptr);

private:
    void onResourcesAdded(const QnResourceList& resources);
};

} // namespace nx::vms::client::desktop
