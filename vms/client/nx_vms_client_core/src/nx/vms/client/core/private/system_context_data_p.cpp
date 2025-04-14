// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_data_p.h"

#include <nx/utils/software_version.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/io_ports/io_ports_compatibility_interface_5_1.h>
#include <nx/vms/client/core/io_ports/io_ports_compatibility_interface_latest.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/thumbnails/remote_async_image_provider.h>

namespace nx::vms::client::core {

void SystemContext::Private::initializeIoPortsInterface()
{
    if (q->moduleInformation().id.isNull())
    {
        ioPortsInterface.reset();
        return;
    }

    static const auto kNewIoPortsApiVersion = nx::utils::SoftwareVersion(6, 1);
    if (q->moduleInformation().version < kNewIoPortsApiVersion)
        ioPortsInterface = std::make_unique<IoPortsCompatibilityInterface_5_1>(q);
    else
        ioPortsInterface = std::make_unique<IoPortsCompatibilityInterface_latest>(q);
}

} // namespace nx::vms::client::core
