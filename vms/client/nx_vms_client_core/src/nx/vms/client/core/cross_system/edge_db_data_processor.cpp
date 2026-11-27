// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edge_db_data_processor.h"

#include <ranges>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx_ec/data/api_conversion_functions.h>

#include "edge_db_data_loader.h"

namespace nx::vms::client::core {

namespace {

using DeviceModel = EdgeDbDataLoader::DeviceModelList::value_type;
using LayoutModel = EdgeDbDataLoader::LayoutDataList::value_type;

using SiteData = EdgeDbDataLoader::SiteData;
using SiteContext = EdgeDbDataProcessor::SiteContext;
using ContextResolver = EdgeDbDataProcessor::ContextResolver;

const ContextResolver kDefaultResolver = [](const QString& siteId) -> SiteContext
{
    const auto manager = appContext()->cloudCrossSystemManager();
    if (!manager)
        return {};

    const auto context = manager->systemContext(siteId);
    if (!context)
        return {};

    return {
        .systemContext = context->systemContext(),
        .hasOwnDevices = context->status() == CloudCrossSystemContext::Status::connected,
    };
};

/** Temporary replacement, which is filled with full data once the Site is connected. */
CrossSystemCameraResourcePtr createFakeDevice(const QString& siteId, const DeviceModel& model)
{
    CrossSystemCameraResourcePtr device(
        new CrossSystemCameraResource(siteId, model.id, model.name, model.typeId));

    // Recorder channels or multisensor camera sensors. The name is the user-defined one if set.
    if (model.group)
    {
        device->setGroupId(model.group->id);
        device->setDefaultGroupName(model.group->name);
    }

    return device;
}

/**
 * Stores the Device parameters. For a new Device it is done before the Device is added, like the
 * initial data sync does, so the resource tree places the Device into its custom group at once
 * instead of moving it there. An existing Device has to notify about the changes itself.
 */
void storeProperties(QnResourcePropertyDictionary* properties,
    const DeviceModel& model,
    const QnResourcePtr& existing)
{
    for (const auto& parameter: model.asList(model.id))
    {
        if (existing)
            existing->setProperty(parameter.name, parameter.value, /*markDirty*/ false);
        else
            properties->setValue(model.id, parameter.name, parameter.value, /*markDirty*/ false);
    }
}

// TODO: #esobolev Load the items, e.g. from edgeDb `layouts/details`, when the Layout is opened.
/** Layout without items, which are not loaded. */
LayoutResourcePtr createFakeLayout(const LayoutModel& model)
{
    LayoutResourcePtr layout(new LayoutResource());
    QnLayoutResourcePtr target = layout;
    ec2::fromApiToResource(model, target);
    layout->addFlags(Qn::remote | Qn::fake);

    return layout;
}

/**
 * Creates the resources missing in the Site resource pool and stores their parameters. Includes
 * the fake Devices for the existing Devices without the Site data, so the pool updates them.
 * edgeDb records are unique by the Site and id, so the records of one Site have different ids.
 */
QnResourceList createSiteResources(
    const SiteContext& context, const QString& siteId, const SiteData& site)
{
    const auto resourcePool = context.systemContext->resourcePool();
    const auto properties = context.systemContext->resourcePropertyDictionary();

    QnResourceList resources;

    // A connected Site keeps its Devices itself and does not expect replacements in its pool.
    if (!context.hasOwnDevices)
    {
        for (const auto& model: site.devices)
        {
            // A Device loaded from the Site has more data than edgeDb gives, so it is not changed.
            // It is present here if the Site was connected before. `Qn::fake` can't be checked
            // instead: such a Device gets it too while its Site is not ready.
            const auto existing =
                resourcePool->getResourceById<CrossSystemCameraResource>(model.id);
            if (existing && existing->source())
                continue;

            // An existing Device without the Site data, e.g. created for a cross-site Layout item
            // or by the previous load, is updated by the pool with the fake one.
            storeProperties(properties, model, existing);
            const auto device = createFakeDevice(siteId, model);

            // The pool does not update the type of an existing camera.
            if (existing)
                existing->setTypeId(device->getTypeId());

            resources.push_back(device);
        }
    }

    // No need to update existing Layouts with partial data.
    for (const auto& model: site.layouts)
    {
        if (!resourcePool->getResourceById(model.id))
            resources.push_back(createFakeLayout(model));
    }

    return resources;
}

} // namespace

EdgeDbDataProcessor::EdgeDbDataProcessor(const EdgeDbDataLoader* loader, QObject* parent):
    EdgeDbDataProcessor(loader, kDefaultResolver, parent)
{
    if (const auto manager = appContext()->cloudCrossSystemManager())
    {
        connect(manager,
            &CloudCrossSystemManager::systemFound,
            this,
            &EdgeDbDataProcessor::onSiteContextAdded);
    }
}

EdgeDbDataProcessor::EdgeDbDataProcessor(
    const EdgeDbDataLoader* loader, ContextResolver contextResolver, QObject* parent):
    QObject(parent),
    m_loader(loader),
    m_contextResolver(std::move(contextResolver))
{
    connect(m_loader, &EdgeDbDataLoader::ready, this, &EdgeDbDataProcessor::onDataReady);
}

void EdgeDbDataProcessor::onDataReady()
{
    // TODO: #esobolev On reload, remove the resources of the previous load that are still fake and
    // absent in the new data, and update the existing Layouts. Now deleted Devices stay until
    // their Site is connected, deleted Layouts stay until the Cloud session ends, and renamed
    // Layouts keep the old names.

    // TODO: #esobolev Consider adding the resources in a worker thread or in portions, one per
    // event loop iteration, for large Organizations: 21000 Devices of 210 Sites block the UI
    // thread for about 2 s.
    for (const auto& siteId: m_loader->sites() | std::views::keys)
        onSiteContextAdded(siteId);
}

void EdgeDbDataProcessor::onSiteContextAdded(const QString& siteId)
{
    const auto& sites = m_loader->sites();
    const auto it = sites.find(siteId);
    if (it == sites.end())
        return;

    const auto context = m_contextResolver(siteId);
    if (!context.systemContext)
    {
        NX_DEBUG(this, "No context for the Site %1, waiting for it", siteId);
        return;
    }

    const auto resources = createSiteResources(context, siteId, it->second);
    if (resources.empty())
        return;

    NX_DEBUG(this, "Adding or updating %1 resources of the Site %2", resources.size(), siteId);
    context.systemContext->resourcePool()->addResources(resources);
}

} // namespace nx::vms::client::core
