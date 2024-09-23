// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/property_storage/filesystem_backend.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Stores properties in separate files in the Local Application Data directory, in the subfolder
 * corresponding to the System ID.
 */
class SystemSpecificFileSystemBackend:
    public QObject,
    public nx::utils::property_storage::FileSystemBackend,
    public SystemContextAware
{
public:
    SystemSpecificFileSystemBackend(SystemContext* systemContext, QObject* parent = nullptr);
};

} // namespace nx::vms::client::desktop
