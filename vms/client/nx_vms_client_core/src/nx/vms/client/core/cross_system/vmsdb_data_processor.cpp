// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vmsdb_data_processor.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/resource/traits.h>

#include "vmsdb_data_loader.h"

namespace nx::vms::client::core {

namespace {

template<typename Data>
bool lessSiteId(const Data& l, const Data& r)
{
    return l.siteId < r.siteId;
}

// TODO: #amalov Get actual type id from VMSDB.
const auto kPendingDeviceTypeId = nx::vms::api::ResourceData::getFixedTypeId("PendingDevice");

} // namespace

void VmsDbDataProcessor::onDataReady(const VmsDbDataLoader* loader)
{
    auto devices = loader->devices();
    if (devices.empty())
        return;

    using Data = decltype(devices)::value_type;

    std::ranges::sort(devices, lessSiteId<Data>);

    auto start = devices.begin(); //< Start of the range with the same siteId.
    while (start != devices.end())
    {
        // End of the range with the same siteId.
        const auto end = std::upper_bound(start, devices.end(), *start, lessSiteId<Data>);
        auto context = appContext()->cloudCrossSystemManager()->systemContext(start->siteId);

        if (context)
        {
            auto system = context->systemContext();
            auto resourcePool = system->resourcePool();

            for (auto it = start; it != end; ++it)
            {
                // No need to update existing resources with partial data.
                if (!resourcePool->getResourceById(it->id))
                {
                    // TODO: #amalov Determine minimum data field set to create good-looking fake
                    // device without assertions.
                    auto fake = appContext()->resourceFactory()->createResource(
                        kPendingDeviceTypeId, {});

                    if (!NX_ASSERT(fake))
                        continue;

                    // TODO: #amalov Consider using generic resource traits.
                    fake->setIdUnsafe(it->id);
                    fake->setName(it->name);
                    fake->addFlags(Qn::fake);

                    resourcePool->addResource(fake);

                    // Status & parameters may be set only after adding to the pool.
                    fake->setStatus(it->status.value(), Qn::StatusChangeReason::GotFromRemotePeer);
                    for (const auto& [name, value]: it->parameters)
                        fake->setProperty(name, value.toJson(), /*markDirty*/ false);
                }
            }

            // TODO: #amalov Implement batch addition, think about changing properties.
        }

        start = end;
    }
}

} // namespace nx::vms::client::core
