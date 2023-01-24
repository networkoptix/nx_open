// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QString>

#include <nx/utils/property_storage/storage.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Settings which are stored locally on the client side but specific to the given System.
 */
class SystemSpecificLocalSettings:
    public nx::utils::property_storage::Storage,
    public SystemContextAware
{
    Q_OBJECT

public:
    SystemSpecificLocalSettings(SystemContext* systemContext);
};

} // namespace nx::vms::client::desktop
